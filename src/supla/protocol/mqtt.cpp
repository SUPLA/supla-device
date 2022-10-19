/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "mqtt.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <supla/storage/config.h>
#include <supla/log_wrapper.h>
#include <SuplaDevice.h>
#include <supla/mutex.h>
#include <supla/auto_lock.h>
#include <supla/time.h>
#include <supla/channel.h>
#include <supla/network/network.h>
#include <supla/tools.h>
#include <supla/element.h>
#include <supla/protocol/mqtt_topic.h>

Supla::Protocol::Mqtt::Mqtt(SuplaDeviceClass *sdc) :
  Supla::Protocol::ProtocolLayer(sdc) {
}

Supla::Protocol::Mqtt::~Mqtt() {
  if (prefix) {
    delete[] prefix;
    prefix = nullptr;
    prefixLen = 0;
  }
}

bool Supla::Protocol::Mqtt::onLoadConfig() {
  // MQTT protocol specific config
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg == nullptr) {
    return false;
  }

  bool configComplete = true;

  if (cfg->isMqttCommProtocolEnabled()) {
    enabled = true;
    if (!cfg->getMqttServer(server) || strlen(server) <= 0) {
      SUPLA_LOG_INFO("Config incomplete: missing MQTT server");
      configComplete = false;
    } else {
      configEmpty = false;
    }

    useTls = cfg->isMqttTlsEnabled();
    port = cfg->getMqttServerPort();
    retainCfg = cfg->isMqttRetainEnabled();
    qosCfg = cfg->getMqttQos();
    useAuth = cfg->isMqttAuthEnabled();

    if (useAuth) {
      if (!cfg->getMqttUser(user) || strlen(user) <= 0) {
        SUPLA_LOG_INFO("Config incomplete: missing MQTT username");
        configComplete = false;
      } else {
        configEmpty = false;
      }
      if (!cfg->getMqttPassword(password) || strlen(password) <= 0) {
        SUPLA_LOG_INFO("Config incomplete: missing MQTT password");
        configComplete = false;
      }
    }
  } else {
    enabled = false;
  }

  return configComplete;
}

bool Supla::Protocol::Mqtt::verifyConfig() {
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg && !cfg->isMqttCommProtocolEnabled()) {
    // skip verification of Mqtt protocol if it is disabled
    return true;
  }

  if (strlen(server) <= 0) {
    sdc->status(STATUS_UNKNOWN_SERVER_ADDRESS, "MQTT: Missing server address");
    if (sdc->getDeviceMode() != Supla::DEVICE_MODE_CONFIG) {
      return false;
    }
  }

  if (useAuth) {
    if (strlen(user) <= 0) {
      sdc->status(STATUS_MISSING_CREDENTIALS, "MQTT: Missing username");
      if (sdc->getDeviceMode() != Supla::DEVICE_MODE_CONFIG) {
        return false;
      }
    }
    if (strlen(password) <= 0) {
      sdc->status(STATUS_MISSING_CREDENTIALS, "MQTT: Missing password");
      if (sdc->getDeviceMode() != Supla::DEVICE_MODE_CONFIG) {
        return false;
      }
    }
  }
  return true;
}


bool Supla::Protocol::Mqtt::isNetworkRestartRequested() {
  return false;
}

uint32_t Supla::Protocol::Mqtt::getConnectionFailTime() {
  return 0;
}

// By default MQTT protocol is enabled.
// It can be disabled if we have config class instance and when it is
// disabled there.
bool Supla::Protocol::Mqtt::isEnabled() {
  return enabled;
}

bool Supla::Protocol::Mqtt::isConnectionError() {
  return error;
}

bool Supla::Protocol::Mqtt::isConnecting() {
  return connecting;
}

void Supla::Protocol::Mqtt::generateClientId(
    char result[MQTT_CLIENTID_MAX_SIZE]) {
  memset(result, 0, MQTT_CLIENTID_MAX_SIZE);

  // GUID is truncated here, because of client_id parameter limitation
  snprintf(
      result, MQTT_CLIENTID_MAX_SIZE,
      "SUPLA-%02X%02X%02X%02X%02X%02X%02X%02X",
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[0]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[1]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[2]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[3]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[4]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[5]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[6]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[7]));
}

void Supla::Protocol::Mqtt::onInit() {
  auto cfg = Supla::Storage::ConfigInstance();

  char customPrefix[49] = {};
  if (cfg != nullptr) {
    cfg->getMqttPrefix(customPrefix);
  }
  int customPrefixLength = strnlen(customPrefix, 48);
  if (customPrefixLength > 0) {
    customPrefixLength++;  // add one char for '/'
  }
  sdc->generateHostname(hostname, 3);
  for (int i = 0; i < 32; i++) {
    hostname[i] = static_cast<char>(tolower(hostname[i]));
  }
  int hostnameLength = strlen(hostname);
  char suplaTopic[] = "supla/devices/";
  int length = customPrefixLength + hostnameLength
    + strlen(suplaTopic) + 1;
  if (prefix) {
    delete[] prefix;
    prefix = nullptr;
    prefixLen = 0;
  }
  prefix = new char[length];
  prefixLen = length - 1;
  if (prefix) {
    snprintf(prefix, length, "%s%s%s%s",
        customPrefix,
        customPrefixLength > 0 ? "/" : "",
        suplaTopic,
        hostname);
    SUPLA_LOG_DEBUG("Mqtt: generated prefix (%d) \"%s\"", prefixLen, prefix);
  } else {
    SUPLA_LOG_ERROR("Mqtt: failed to generate prefix");
  }

  channelsCount = Supla::Channel::reg_dev.channel_count;
}

void Supla::Protocol::Mqtt::publishDeviceStatus(bool onRegistration) {
  buttonNumber = 0;
  TDSC_ChannelState channelState = {};
  Supla::Network::Instance()->fillStateData(&channelState);

  if (onRegistration) {
    publishBool("state/connected", true, -1, 1);
    uint8_t mac[6] = {};
    char macStr[12 + 6] = {};
    if (Supla::Network::GetMacAddr(mac)) {
      generateHexString(mac, macStr, 6, ':');
    }

    publish("state/mac", macStr);

    char ipStr[17] = {};
    uint8_t ip[4] = {};
    memcpy(ip, &channelState.IPv4, 4);
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    publish("state/ip", ipStr);
  }

  publishInt("state/uptime", uptime.getUptime());
  if (!sdc->isSleepingDeviceEnabled()) {
    publishInt("state/connection_uptime", uptime.getConnectionUptime());
  }

  int8_t rssi = 0;
  memcpy(&rssi, &channelState.WiFiRSSI, 1);
  publishInt("state/rssi", rssi);
  publishInt("state/wifi_signal_strength", channelState.WiFiSignalStrength);
}

void Supla::Protocol::Mqtt::publish(const char *topic,
                                    const char *payload,
                                    int qos,
                                    int retain,
                                    bool ignorePrefix) {
  if (prefix == nullptr) {
    SUPLA_LOG_ERROR("Mqtt: publish error, prefix not initialized");
    return;
  }

  bool retainValue = retain == 1 ? true : false;
  if (retain == -1) {
    retainValue = retainCfg;
  }

  if (qos == -1) {
    qos = qosCfg;
  }

  MqttTopic mqttTopic;
  if (!ignorePrefix) {
    mqttTopic.append(prefix);
    mqttTopic = mqttTopic / topic;
  } else {
    mqttTopic.append(topic);
  }

  SUPLA_LOG_DEBUG("MQTT publish(qos: %d, retain: %d): \"%s\" - \"%s\"",
      qos,
      retainValue,
      mqttTopic.c_str(),
      payload);
  publishImp(mqttTopic.c_str(), payload, qos, retainValue);
}

void Supla::Protocol::Mqtt::publishInt(const char *topic,
                                    int payload,
                                    int qos,
                                    int retain) {
  char buf[100] = {};
  snprintf(buf, sizeof(buf), "%d", payload);
  publish(topic, buf, qos, retain);
}

void Supla::Protocol::Mqtt::publishBool(const char *topic,
                                    bool payload,
                                    int qos,
                                    int retain) {
  char buf[6] = {};
  snprintf(buf, sizeof(buf), "%s", payload ? "true" : "false");
  publish(topic, buf, qos, retain);
}

void Supla::Protocol::Mqtt::publishDouble(const char *topic,
                                    double payload,
                                    int qos,
                                    int retain) {
  char buf[100] = {};
  snprintf(buf, sizeof(buf), "%.2f", payload);
  publish(topic, buf, qos, retain);
}

void Supla::Protocol::Mqtt::subscribe(const char *topic, int qos) {
  if (prefix == nullptr) {
    SUPLA_LOG_ERROR("Mqtt: publish error, prefix not initialized");
    return;
  }

  if (qos == -1) {
    qos = qosCfg;
  }

  MqttTopic mqttTopic(prefix);
  mqttTopic = mqttTopic / topic;
  SUPLA_LOG_DEBUG("MQTT subscribe(qos: %d): \"%s\"",
      qos,
      mqttTopic.c_str());
  subscribeImp(mqttTopic.c_str(), qos);
}

void Supla::Protocol::Mqtt::publishChannelState(int channel) {
  SUPLA_LOG_DEBUG("Mqtt: publish channel %d state", channel);
  if (channel < 0 || channel >= channelsCount) {
    SUPLA_LOG_WARNING("Mqtt: invalid channel %d for publish", channel);
    return;
  }

  auto topic = MqttTopic("channels") / channel / "state";

  auto element = Supla::Element::getElementByChannelNumber(channel);
  if (element == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: can't find element for channel %d", channel);
    return;
  }
  auto ch = element->getChannel();
  if (ch == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: failed to load channel object");
    return;
  }

  switch (ch->getChannelType()) {
    case SUPLA_CHANNELTYPE_RELAY: {
      // publish relay state
      publishBool((topic / "on").c_str(), ch->getValueBool(), -1, 1);
      break;
    }
    case SUPLA_CHANNELTYPE_THERMOMETER: {
      // publish thermometer state
      if (ch->getValueDouble() > -273) {
        publishDouble((topic / "temperature").c_str(), ch->getValueDouble(),
            -1, 1);
      }
      break;
    }
    case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR: {
      // publish thermometer state
      if (ch->getValueDoubleFirst() > -273) {
        publishDouble((topic / "temperature").c_str(),
            ch->getValueDoubleFirst(), -1, 1);
      }
      if (ch->getValueDoubleSecond() >= 0) {
        publishDouble((topic / "humidity").c_str(), ch->getValueDoubleSecond(),
            -1, 1);
      }
      break;
    }
    default:
      SUPLA_LOG_DEBUG("Mqtt: channel type %d not supported",
          ch->getChannelType());
      break;
  }
}

void Supla::Protocol::Mqtt::subscribeChannel(int channel) {
  SUPLA_LOG_DEBUG("Mqtt: subscribe channel %d", channel);
  if (channel < 0 || channel >= channelsCount) {
    SUPLA_LOG_WARNING("Mqtt: invalid channel %d for subscribe", channel);
    return;
  }

  auto topic = MqttTopic("channels") / channel;

  auto element = Supla::Element::getElementByChannelNumber(channel);
  if (element == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: can't find element for channel %d", channel);
    return;
  }
  auto ch = element->getChannel();
  if (ch == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: failed to load channel object");
    return;
  }

  switch (ch->getChannelType()) {
    case SUPLA_CHANNELTYPE_RELAY: {
      subscribe((topic / "set" / "on").c_str());
      subscribe((topic / "execute_action").c_str());
      break;
    }
    default:
      SUPLA_LOG_DEBUG("Mqtt: channel type %d not supported",
          ch->getChannelType());
      break;
  }
}

bool Supla::Protocol::Mqtt::processData(const char *topic,
                                        const char *payload) {
  if (topic == nullptr) {
    return false;
  }

  if (payload == nullptr) {
    return false;
  }

  SUPLA_LOG_DEBUG("Mqtt data received, topic: \"%s\", payload: \"%s\"", topic,
      payload);

  int topicLen = strlen(topic);
  char channelsString[] = "/channels/";
  int channelsStringLen = strlen(channelsString);

  if (topicLen <= prefixLen || strncmp(topic, prefix, prefixLen) != 0) {
    SUPLA_LOG_DEBUG("Mqtt: topic doesn't match device's prefix");
    return false;
  }

  if (topicLen <= prefixLen + channelsStringLen ||
      strncmp(topic + prefixLen, channelsString, channelsStringLen) != 0) {
    SUPLA_LOG_DEBUG("Mqtt: topic doesn't contain channels path");
    return false;
  }

  char topicCopy[MAX_TOPIC_LEN] = {};
  strncpy(topicCopy, topic, MAX_TOPIC_LEN);

  char *savePtr;
  char *part =
    strtok_r(topicCopy + prefixLen + channelsStringLen, "/", &savePtr);
  int channel = -1;
  if (part == nullptr) {
    return false;
  }
  channel = stringToInt(part);
  auto element = Supla::Element::getElementByChannelNumber(channel);
  if (element == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: can't find element for channel %d", channel);
    return false;
  }

  part = strtok_r(nullptr, "", &savePtr);
  if (part == nullptr) {
    return false;
  }

  auto ch = element->getChannel();
  if (ch == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: failed to load channel object");
    return false;
  }

  TSD_SuplaChannelNewValue newValue = {};
  element->fillSuplaChannelNewValue(&newValue);

  switch (ch->getChannelType()) {
    // Relay
    case SUPLA_CHANNELTYPE_RELAY: {
      if (strcmp(part, "set/on") == 0) {
        if (isPayloadOn(payload)) {
          newValue.value[0] = 1;
        } else {
          newValue.value[0] = 0;
        }
        element->handleNewValueFromServer(&newValue);
      } else if (strcmp(part, "execute_action") == 0) {
        if (strncmpInsensitive(payload, "turn_on", 8) == 0) {
          newValue.value[0] = 1;
          element->handleNewValueFromServer(&newValue);
        } else if (strncmpInsensitive(payload, "turn_off", 9) == 0) {
          newValue.value[0] = 0;
          element->handleNewValueFromServer(&newValue);
        } else if (strncmpInsensitive(payload, "toggle", 7) == 0) {
          newValue.value[0] = ch->getValueBool() ? 0 : 1;
          element->handleNewValueFromServer(&newValue);
        } else {
          SUPLA_LOG_DEBUG("Mqtt: unsupported action %s", payload);
        }
      } else {
        SUPLA_LOG_DEBUG("Mqtt: received unsupported topic %s", part);
      }
      break;
    }
    // TODO(klew): add here more channel types

    // Not supported
    default:
      SUPLA_LOG_DEBUG("Mqtt: channel type %d not supported",
          ch->getChannelType());
      break;
  }
  return true;
}

bool Supla::Protocol::Mqtt::isPayloadOn(const char *payload) {
  if (payload == nullptr) {
    return false;
  }

  // compare 2 bytes, i.e. it has to match "1\0"
  if (strncmpInsensitive(payload, "1", 2) == 0) {
    return true;
  }

  if (strncmpInsensitive(payload, "yes", 4) == 0) {
    return true;
  }

  if (strncmpInsensitive(payload, "true", 5) == 0) {
    return true;
  }

  return false;
}

void Supla::Protocol::Mqtt::publishHADiscovery(int channel) {
  SUPLA_LOG_DEBUG("Mqtt: publish HA discovery for channel %d", channel);
  if (channel < 0 || channel >= channelsCount) {
    SUPLA_LOG_WARNING("Mqtt: invalid channel %d for publish", channel);
    return;
  }

  auto element = Supla::Element::getElementByChannelNumber(channel);
  if (element == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: can't find element for channel %d", channel);
    return;
  }
  auto ch = element->getChannel();
  if (ch == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: failed to load channel object");
    return;
  }

  switch (ch->getChannelType()) {
    case SUPLA_CHANNELTYPE_RELAY: {
      // publish relay state
      publishHADiscoveryRelay(element);
      break;
    }
    case SUPLA_CHANNELTYPE_THERMOMETER: {
      publishHADiscoveryThermometer(element);
      break;
    }
    case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR: {
      publishHADiscoveryThermometer(element);
      publishHADiscoveryHumidity(element);
      break;
    }
    case SUPLA_CHANNELTYPE_ACTIONTRIGGER: {
      publishHADiscoveryActionTrigger(element);
      break;
    }
    default:
      SUPLA_LOG_DEBUG("Mqtt: channel type %d not supported",
          ch->getChannelType());
      break;
  }
}

void Supla::Protocol::Mqtt::publishHADiscoveryRelay(Supla::Element *element) {
  if (element == nullptr) {
    return;
  }

  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  bool lightType = true;

  if (ch->getDefaultFunction() == SUPLA_CHANNELFNC_POWERSWITCH) {
    lightType = false;
  }

  char objectId[30] = {};
  generateObjectId(objectId, element->getChannelNumber(), 0);

  auto topic = getHADiscoveryTopic(lightType ? "light" : "switch", objectId);

  const char cfg[] =
      "{"
      "\"avty_t\":\"%s/state/connected\","
      "\"pl_avail\":\"true\","
      "\"pl_not_avail\":\"false\","
      "\"~\":\"%s/channels/%i\","
      "\"dev\":{"
        "\"ids\":\"%s\","
        "\"mf\":\"%s\","
        "\"name\":\"%s\","
        "\"sw\":\"%s\""
      "},"
      "\"name\":\"#%i %s switch\","
      "\"uniq_id\":\"supla_%s\","
      "\"qos\":0,"
      "\"ret\":false,"
      "\"opt\":false,"
      "\"stat_t\":\"~/state/on\","
      "\"cmd_t\":\"~/set/on\","
      "\"pl_on\":\"true\","
      "\"pl_off\":\"false\""
      "}";

  char c = '\0';

  size_t bufferSize = 0;
  char *payload = {};

  for (int i = 0; i < 2; i++) {
    bufferSize =
        snprintf(i ? payload : &c, i ? bufferSize : 1,
            cfg,
            prefix,
            prefix,
            ch->getChannelNumber(),
            hostname,
            getManufacturer(Supla::Channel::reg_dev.ManufacturerID),
            Supla::Channel::reg_dev.Name,
            Supla::Channel::reg_dev.SoftVer,
            element->getChannelNumber(),
            lightType ? "Light" : "Power",
            objectId)
        + 1;

    if (i == 0) {
      payload = new char[bufferSize];
      if (payload == nullptr) {
        return;
      }
    }
  }

  publish(topic.c_str(), payload, -1, 1, true);

  delete[] payload;
}

void Supla::Protocol::Mqtt::publishHADiscoveryThermometer(
    Supla::Element *element) {
  if (element == nullptr) {
    return;
  }

  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  char objectId[30] = {};
  int subId = ch->getChannelType() == SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR ?
    1 : 0;
  generateObjectId(objectId, element->getChannelNumber(), subId);

  auto topic = getHADiscoveryTopic("sensor", objectId);

  const char cfg[] =
      "{"
      "\"avty_t\":\"%s/state/connected\","
      "\"pl_avail\":\"true\","
      "\"pl_not_avail\":\"false\","
      "\"~\":\"%s/channels/%i\","
      "\"dev\":{"
        "\"ids\":\"%s\","
        "\"mf\":\"%s\","
        "\"name\":\"%s\","
        "\"sw\":\"%s\""
      "},"
      "\"name\":\"#%i Temperature\","
      "\"uniq_id\":\"supla_%s\","
      "\"dev_cla\":\"temperature\","
      "\"unit_of_meas\":\"°C\","
      "\"stat_cla\":\"measurement\","
      "\"expire_after\":%d,"
      "\"qos\":0,"
      "\"ret\":false,"
      "\"opt\":false,"
      "\"stat_t\":\"~/state/temperature\""
      "}";

  char c = '\0';

  size_t bufferSize = 0;
  char *payload = {};

  for (int i = 0; i < 2; i++) {
    bufferSize =
        snprintf(i ? payload : &c, i ? bufferSize : 1,
            cfg,
            prefix,
            prefix,
            ch->getChannelNumber(),
            hostname,
            getManufacturer(Supla::Channel::reg_dev.ManufacturerID),
            Supla::Channel::reg_dev.Name,
            Supla::Channel::reg_dev.SoftVer,
            element->getChannelNumber(),
            objectId,
            static_cast<int>(sdc->getActivityTimeout())
            )
        + 1;

    if (i == 0) {
      payload = new char[bufferSize];
      if (payload == nullptr) {
        return;
      }
    }
  }

  publish(topic.c_str(), payload, -1, 1, true);

  delete[] payload;
}

void Supla::Protocol::Mqtt::generateObjectId(char *result, int channelNumber,
    int subId) {
  uint8_t mac[6] = {};
  if (channelNumber >= 100 || subId >= 100
      || channelNumber < 0 || subId < 0) {
    SUPLA_LOG_DEBUG("Mqtt: invalid channel number");
    return;
  }
  Supla::Network::GetMacAddr(mac);
  generateHexString(mac, result, 6);
  for (int i = 0; i < 12; i++) {
    result[i] = static_cast<char>(tolower(result[i]));
  }
  int size = 7;

  snprintf(result + 12, size, "_%d_%d", channelNumber, subId);
}

Supla::Protocol::MqttTopic Supla::Protocol::Mqtt::getHADiscoveryTopic(
    const char *type, char *objectId) {
  // "homeassistant/%s/supla/%02x%02x%02x%02x%02x%02x_%i_%i/config";
  MqttTopic topic("homeassistant");
  topic = topic / type / "supla" / objectId / "config";
  return topic;
}

void Supla::Protocol::Mqtt::publishHADiscoveryHumidity(
    Supla::Element *element) {
  if (element == nullptr) {
    return;
  }

  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  char objectId[30] = {};
  generateObjectId(objectId, element->getChannelNumber(), 0);

  auto topic = getHADiscoveryTopic("sensor", objectId);

  const char cfg[] =
      "{"
      "\"avty_t\":\"%s/state/connected\","
      "\"pl_avail\":\"true\","
      "\"pl_not_avail\":\"false\","
      "\"~\":\"%s/channels/%i\","
      "\"dev\":{"
        "\"ids\":\"%s\","
        "\"mf\":\"%s\","
        "\"name\":\"%s\","
        "\"sw\":\"%s\""
      "},"
      "\"name\":\"#%i Humidity\","
      "\"dev_cla\":\"humidity\","
      "\"stat_cla\":\"measurement\","
      "\"unit_of_meas\":\"%%\","
      "\"uniq_id\":\"supla_%s\","
      "\"expire_after\":%d,"
      "\"qos\":0,"
      "\"ret\":false,"
      "\"opt\":false,"
      "\"stat_t\":\"~/state/humidity\""
      "}";

  char c = '\0';

  size_t bufferSize = 0;
  char *payload = {};

  for (int i = 0; i < 2; i++) {
    bufferSize =
        snprintf(i ? payload : &c, i ? bufferSize : 1,
            cfg,
            prefix,
            prefix,
            ch->getChannelNumber(),
            hostname,
            getManufacturer(Supla::Channel::reg_dev.ManufacturerID),
            Supla::Channel::reg_dev.Name,
            Supla::Channel::reg_dev.SoftVer,
            element->getChannelNumber(),
            objectId,
            static_cast<int>(sdc->getActivityTimeout()))
        + 1;

    if (i == 0) {
      payload = new char[bufferSize];
      if (payload == nullptr) {
        return;
      }
    }
  }

  publish(topic.c_str(), payload, -1, 1, true);

  delete[] payload;
}

const char *Supla::Protocol::Mqtt::getActionTriggerType(uint8_t actionIdx) {
  switch (actionIdx) {
    case 0:
      return "button_long_press";
    case 1:
      return "button_short_press";
    case 2:
      return "button_double_press";
    case 3:
      return "button_triple_press";
    case 4:
      return "button_quadruple_press";
    case 5:
      return "button_quintuple_press";
    case 6:
      return "button_turn_on";
    case 7:
      return "button_turn_off";
    default:
      return "undefined";
  }
}

bool Supla::Protocol::Mqtt::isActionTriggerEnabled(
    Supla::Channel *ch, uint8_t actionIdx) {
  if (ch == nullptr) {
    return false;
  }
  if (actionIdx > 7) {
    return false;
  }

  auto atCaps = ch->getActionTriggerCaps();

  switch (actionIdx) {
    case 0:
      return (atCaps & SUPLA_ACTION_CAP_HOLD);
    case 1:
      return (atCaps & SUPLA_ACTION_CAP_TOGGLE_x1)
        || (atCaps & SUPLA_ACTION_CAP_SHORT_PRESS_x1);
    case 2:
      return (atCaps & SUPLA_ACTION_CAP_TOGGLE_x2)
        || (atCaps & SUPLA_ACTION_CAP_SHORT_PRESS_x2);
    case 3:
      return (atCaps & SUPLA_ACTION_CAP_TOGGLE_x3)
        || (atCaps & SUPLA_ACTION_CAP_SHORT_PRESS_x3);
    case 4:
      return (atCaps & SUPLA_ACTION_CAP_TOGGLE_x4)
        || (atCaps & SUPLA_ACTION_CAP_SHORT_PRESS_x4);
    case 5:
      return (atCaps & SUPLA_ACTION_CAP_TOGGLE_x5)
        || (atCaps & SUPLA_ACTION_CAP_SHORT_PRESS_x5);
    case 6:
      return (atCaps & SUPLA_ACTION_CAP_TURN_ON);
    case 7:
      return (atCaps & SUPLA_ACTION_CAP_TURN_OFF);
  }
  return false;
}

void Supla::Protocol::Mqtt::publishHADiscoveryActionTrigger(
    Supla::Element *element) {
  if (element == nullptr) {
    return;
  }

  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  buttonNumber++;

  for (int actionIdx = 0; actionIdx < 8; actionIdx++) {
    char objectId[30] = {};
    generateObjectId(objectId, element->getChannelNumber(), actionIdx);

    auto topic = getHADiscoveryTopic("device_automation", objectId);

    bool enabled = isActionTriggerEnabled(ch, actionIdx);

    if (enabled) {
      const char cfg[] =
        "{"
        "\"dev\":{"
          "\"ids\":\"%s\","
          "\"mf\":\"%s\","
          "\"name\":\"%s\","
          "\"sw\":\"%s\""
        "},"
        "\"automation_type\":\"trigger\","
        "\"topic\":\"%s/channels/%i/%s\","
        "\"type\":\"%s\","
        "\"subtype\":\"button_%i\","
        "\"payload\":\"%s\","
        "\"qos\":0,"
        "\"ret\":false"
        "}";

      char c = '\0';

      size_t bufferSize = 0;
      char *payload = {};
      const char* atType = getActionTriggerType(actionIdx);

      for (int i = 0; i < 2; i++) {
        bufferSize =
          snprintf(i ? payload : &c, i ? bufferSize : 1,
              cfg,
              hostname,
              getManufacturer(Supla::Channel::reg_dev.ManufacturerID),
              Supla::Channel::reg_dev.Name,
              Supla::Channel::reg_dev.SoftVer,
              prefix,
              ch->getChannelNumber(),
              atType,
              atType,
              buttonNumber,
              atType
              )
          + 1;

        if (i == 0) {
          payload = new char[bufferSize];
          if (payload == nullptr) {
            return;
          }
        }
      }

      publish(topic.c_str(), payload, -1, 1, true);

      delete[] payload;
    } else {
      // disabled action
      publish(topic.c_str(), "", -1, 1, true);
    }
  }
}

bool Supla::Protocol::Mqtt::isUpdatePending() {
  return Supla::Element::IsAnyUpdatePending();
}

void Supla::Protocol::Mqtt::sendActionTrigger(
    uint8_t channelNumber, uint32_t actionId) {
  if (!isRegisteredAndReady()) {
    return;
  }

  uint8_t actionIdx = 0;
  switch (actionId) {
    case SUPLA_ACTION_CAP_HOLD:
      actionIdx = 0;
      break;
    case SUPLA_ACTION_CAP_TOGGLE_x1:
    case SUPLA_ACTION_CAP_SHORT_PRESS_x1:
      actionIdx = 1;
      break;
    case SUPLA_ACTION_CAP_TOGGLE_x2:
    case SUPLA_ACTION_CAP_SHORT_PRESS_x2:
      actionIdx = 2;
      break;
    case SUPLA_ACTION_CAP_TOGGLE_x3:
    case SUPLA_ACTION_CAP_SHORT_PRESS_x3:
      actionIdx = 3;
      break;
    case SUPLA_ACTION_CAP_TOGGLE_x4:
    case SUPLA_ACTION_CAP_SHORT_PRESS_x4:
      actionIdx = 4;
      break;
    case SUPLA_ACTION_CAP_TOGGLE_x5:
    case SUPLA_ACTION_CAP_SHORT_PRESS_x5:
      actionIdx = 5;
      break;
    case SUPLA_ACTION_CAP_TURN_ON:
      actionIdx = 6;
      break;
    case SUPLA_ACTION_CAP_TURN_OFF:
      actionIdx = 7;
      break;

    default:
      SUPLA_LOG_WARNING("Mqtt: invalid actionId %d", actionId);
      actionIdx = 8;
      break;
  }
  const char *actionString = getActionTriggerType(actionIdx);
  auto topic = MqttTopic("channels") / channelNumber / actionString;
  publish(topic.c_str(), actionString, -1, 0);
}

bool Supla::Protocol::Mqtt::isRegisteredAndReady() {
  return connected;
}

void Supla::Protocol::Mqtt::sendChannelValueChanged(
    uint8_t channelNumber,
    char *value,
    unsigned char offline,
    uint32_t validityTimeSec) {
  (void)(value);
  (void)(offline);
  (void)(validityTimeSec);
  if (!isRegisteredAndReady()) {
    return;
  }

  publishChannelState(channelNumber);
}

// TODO(klew): implement for EM
void Supla::Protocol::Mqtt::sendExtendedChannelValueChanged(
    uint8_t channelNumber,
    TSuplaChannelExtendedValue *value) {
  (void)(channelNumber);
  (void)(value);
  if (!isRegisteredAndReady()) {
    return;
  }

  // send value
}
