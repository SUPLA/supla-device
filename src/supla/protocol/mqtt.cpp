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
#include <supla/sensor/electricity_meter.h>
#include <supla/control/hvac_base.h>

using Supla::Protocol::Mqtt;

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
  if (!enabled) {
    return;
  }

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
                                    int retain,
                                    int precision) {
  char buf[100] = {};
  snprintf(buf, sizeof(buf), "%.*f", precision, payload);
  publish(topic, buf, qos, retain);
}

void Mqtt::publishColor(const char *topic,
                        uint8_t red,
                        uint8_t green,
                        uint8_t blue,
                        int qos,
                        int retain) {
  char buf[12] = {};
  snprintf(buf, sizeof(buf), "%d,%d,%d", red, green, blue);
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
    case SUPLA_CHANNELTYPE_ELECTRICITY_METER: {
      // Data for EM is published by publishExtendedChannelState method
      break;
    }
    case SUPLA_CHANNELTYPE_DIMMER: {
      // publish dimmer state
      publishInt(
          (topic / "brightness").c_str(), ch->getValueBrightness(), -1, 1);
      publishBool((topic / "on").c_str(),
                  ch->getValueBrightness() > 0,
                  -1,
                  1);
      break;
    }
    case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER: {
      // publish RGB LED state
      publishInt((topic / "color_brightness").c_str(),
                 ch->getValueColorBrightness(),
                 -1,
                 1);
      publishBool((topic / "on").c_str(),
                  ch->getValueColorBrightness() > 0,
                  -1,
                  1);
      publishColor((topic / "color").c_str(),
                   ch->getValueRed(),
                   ch->getValueGreen(),
                   ch->getValueBlue(),
                   -1,
                   1);
      break;
    }

    case SUPLA_CHANNELTYPE_DIMMERANDRGBLED: {
      // publish dimmer and RGB LED state
      publishInt(
          (topic / "brightness").c_str(), ch->getValueBrightness(), -1, 1);
      publishInt((topic / "color_brightness").c_str(),
                 ch->getValueColorBrightness(),
                 -1,
                 1);
      publishBool((topic / "rgb" / "on").c_str(),
                  ch->getValueColorBrightness() > 0,
                  -1,
                  1);
      publishBool((topic / "dimmer" / "on").c_str(),
                  ch->getValueBrightness() > 0,
                  -1,
                  1);
      publishColor((topic / "color").c_str(),
                   ch->getValueRed(),
                   ch->getValueGreen(),
                   ch->getValueBlue(),
                   -1,
                   1);
      break;
    }

    case SUPLA_CHANNELTYPE_HVAC: {
      if (ch->isHvacFlagHeating()) {
        publish((topic / "action").c_str(), "heating", -1, 1);
      } else if (ch->isHvacFlagCooling()) {
        publish((topic / "action").c_str(), "cooling", -1, 1);
      } else if (ch->getHvacMode() == SUPLA_HVAC_MODE_OFF ||
          ch->getHvacMode() == SUPLA_HVAC_MODE_NOT_SET) {
        publish((topic / "action").c_str(), "off", -1, 1);
      } else {
        publish((topic / "action").c_str(), "idle", -1, 1);
      }
      if (ch->isHvacFlagWeeklySchedule()) {
        publish((topic / "mode").c_str(), "auto", -1, 1);
      } else {
        switch (ch->getHvacMode()) {
          case SUPLA_HVAC_MODE_HEAT: {
            publish((topic / "mode").c_str(), "heat", -1, 1);
            break;
          }
          case SUPLA_HVAC_MODE_COOL: {
            publish((topic / "mode").c_str(), "cool", -1, 1);
            break;
          }
          case SUPLA_HVAC_MODE_AUTO: {
            publish((topic / "mode").c_str(), "heat_cool", -1, 1);
            break;
          }
          case SUPLA_HVAC_MODE_OFF:
          case SUPLA_HVAC_MODE_NOT_SET:
          default: {
            publish((topic / "mode").c_str(), "off", -1, 1);
            break;
          }
        }
      }
      if (ch->getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO) {
        publish((topic / "temperature_setpoint").c_str(), "", -1, 1);
        int16_t setpointHeat = ch->getHvacSetpointTemperatureHeat();
        if (setpointHeat > INT16_MIN) {
          publishDouble((topic / "temperature_setpoint_heat").c_str(),
                        static_cast<double>(setpointHeat) / 100.0,
                        -1,
                        1,
                        2);
        } else {
          publish((topic / "temperature_setpoint_heat").c_str(), "", -1, 1);
        }
        int16_t setpointCool = ch->getHvacSetpointTemperatureCool();
        if (setpointCool > INT16_MIN) {
          publishDouble((topic / "temperature_setpoint_cool").c_str(),
                        static_cast<double>(setpointCool) / 100.0,
                        -1,
                        1,
                        2);
        } else {
          publish((topic / "temperature_setpoint_cool").c_str(), "", -1, 1);
        }
      } else {
        int16_t tempreatureSetpoint = ch->getHvacSetpointTemperatureHeat();
        if (ch->getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
            ch->getHvacFlagCoolSubfunction() ==
                HvacCoolSubfunctionFlag::CoolSubfunction) {
          tempreatureSetpoint = ch->getHvacSetpointTemperatureCool();
        }
        publish((topic / "temperature_setpoint_heat").c_str(), "", -1, 1);
        publish((topic / "temperature_setpoint_cool").c_str(), "", -1, 1);
        publishDouble((topic / "temperature_setpoint").c_str(),
                      static_cast<double>(tempreatureSetpoint) / 100.0,
                      -1,
                      1,
                      2);
      }
      break;
    }

    default:
      SUPLA_LOG_DEBUG("Mqtt: channel type %d not supported",
          ch->getChannelType());
      break;
  }
}

using Supla::Sensor::ElectricityMeter;

void Supla::Protocol::Mqtt::publishExtendedChannelState(int channel) {
  SUPLA_LOG_DEBUG("Mqtt: publish extended channel %d state", channel);
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
  auto extCh = ch->getExtValue();
  if (extCh == nullptr) {
    SUPLA_LOG_DEBUG("Mqtt: failed to load extended channel object");
    return;
  }

  auto topic = MqttTopic("channels") / channel / "state";

  switch (ch->getChannelType()) {
    case SUPLA_CHANNELTYPE_ELECTRICITY_METER: {
      TElectricityMeter_ExtendedValue_V2 extEMValue = {};
      if (!ch->getExtValueAsElectricityMeter(&extEMValue)) {
        SUPLA_LOG_DEBUG("Mqtt: failed to obtain ext EM value");
        return;
      }

      if (ElectricityMeter::isFwdActEnergyUsed(extEMValue)) {
        publishDouble((topic / "total_forward_active_energy").c_str(),
            ElectricityMeter::getTotalFwdActEnergy(extEMValue) / 100000.0,
            -1, -1, 4);
      }

      if (ElectricityMeter::isRvrActEnergyUsed(extEMValue)) {
        publishDouble((topic / "total_reverse_active_energy").c_str(),
            ElectricityMeter::getTotalRvrActEnergy(extEMValue) / 100000.0,
            -1, -1, 4);
      }

      for (int phase = 0; phase < MAX_PHASES; phase++) {
        if ((phase == 0 &&
              ch->getFlags() & SUPLA_CHANNEL_FLAG_PHASE1_UNSUPPORTED) ||
            (phase == 1 &&
             ch->getFlags() & SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED) ||
            (phase == 2 &&
             ch->getFlags() & SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED)
           ) {
          SUPLA_LOG_DEBUG("Mqtt: phase %d disabled, skipping", phase);
          continue;
        }
        auto phaseTopic = topic / "phases" / (phase + 1);
        if (ElectricityMeter::isFwdActEnergyUsed(extEMValue)) {
          publishDouble((phaseTopic / "total_forward_active_energy").c_str(),
              ElectricityMeter::getFwdActEnergy(extEMValue, phase) / 100000.0,
              -1, -1, 4);
        }

        if (ElectricityMeter::isRvrActEnergyUsed(extEMValue)) {
          publishDouble((phaseTopic / "total_reverse_active_energy").c_str(),
              ElectricityMeter::getRvrActEnergy(extEMValue, phase) / 100000.0,
              -1, -1, 4);
        }

        if (ElectricityMeter::isFwdReactEnergyUsed(extEMValue)) {
          publishDouble((phaseTopic / "total_forward_reactive_energy").c_str(),
              ElectricityMeter::getFwdReactEnergy(extEMValue, phase) / 100000.0,
              -1, -1, 4);
        }

        if (ElectricityMeter::isRvrReactEnergyUsed(extEMValue)) {
          publishDouble((phaseTopic / "total_reverse_reactive_energy").c_str(),
              ElectricityMeter::getRvrReactEnergy(extEMValue, phase) / 100000.0,
              -1, -1, 4);
        }

        if (ElectricityMeter::isVoltageUsed(extEMValue)) {
          publishDouble((phaseTopic / "voltage").c_str(),
              ElectricityMeter::getVoltage(extEMValue, phase) / 100.0);
        }
        if (ElectricityMeter::isCurrentUsed(extEMValue)) {
          publishDouble((phaseTopic / "current").c_str(),
              ElectricityMeter::getCurrent(extEMValue, phase) / 1000.0,
              -1, -1, 3);
        }
        if (ElectricityMeter::isPowerActiveUsed(extEMValue)) {
          publishDouble((phaseTopic / "power_active").c_str(),
              ElectricityMeter::getPowerActive(extEMValue, phase) / 100000.0,
              -1, -1, 3);
        }
        if (ElectricityMeter::isPowerReactiveUsed(extEMValue)) {
          publishDouble((phaseTopic / "power_reactive").c_str(),
              ElectricityMeter::getPowerReactive(extEMValue, phase) / 100000.0,
              -1, -1, 3);
        }
        if (ElectricityMeter::isPowerApparentUsed(extEMValue)) {
          publishDouble((phaseTopic / "power_apparent").c_str(),
              ElectricityMeter::getPowerApparent(extEMValue, phase) / 100000.0,
              -1, -1, 3);
        }
        if (ElectricityMeter::isPowerFactorUsed(extEMValue)) {
          publishDouble((phaseTopic / "power_factor").c_str(),
              ElectricityMeter::getPowerFactor(extEMValue, phase) / 1000.0);
        }
        if (ElectricityMeter::isPhaseAngleUsed(extEMValue)) {
          publishDouble((phaseTopic / "phase_angle").c_str(),
              ElectricityMeter::getPhaseAngle(extEMValue, phase) / 10.0,
              -1, -1, 1);
        }
        if (ElectricityMeter::isFreqUsed(extEMValue)) {
          publishDouble((phaseTopic / "frequency").c_str(),
              ElectricityMeter::getFreq(extEMValue) / 100.0);
        }
      }

      break;
    }
    default:
      SUPLA_LOG_DEBUG("Mqtt: channel type %d not supported for extended value",
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
    case SUPLA_CHANNELTYPE_DIMMER: {
      subscribe((topic / "execute_action").c_str());
      subscribe((topic / "set" / "brightness").c_str());
      break;
    }
    case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER: {
      subscribe((topic / "execute_action").c_str());
      subscribe((topic / "set" / "color_brightness").c_str());
      subscribe((topic / "set" / "color").c_str());
      break;
    }
    case SUPLA_CHANNELTYPE_DIMMERANDRGBLED: {
      subscribe((topic / "execute_action" / "rgb").c_str());
      subscribe((topic / "execute_action" / "dimmer").c_str());
      subscribe((topic / "set" / "brightness").c_str());
      subscribe((topic / "set" / "color_brightness").c_str());
      subscribe((topic / "set" / "color").c_str());
      break;
    }
    case SUPLA_CHANNELTYPE_HVAC: {
      subscribe((topic / "execute_action").c_str());
      subscribe((topic / "set" / "temperature_setpoint").c_str());
      subscribe((topic / "set" / "temperature_setpoint_heat").c_str());
      subscribe((topic / "set" / "temperature_setpoint_cool").c_str());
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
  switch (ch->getChannelType()) {
    // Relay
    case SUPLA_CHANNELTYPE_RELAY: {
      processRelayRequest(part, payload, element);
      break;
    }
    case SUPLA_CHANNELTYPE_DIMMER: {
      processDimmerRequest(part, payload, element);
      break;
    }
    case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER: {
      processRGBWRequest(part, payload, element);
      break;
    }
    case SUPLA_CHANNELTYPE_DIMMERANDRGBLED: {
      processRGBWRequest(part, payload, element);
      break;
    }
    case SUPLA_CHANNELTYPE_HVAC: {
      processHVACRequest(part, payload, element);
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
    case SUPLA_CHANNELTYPE_ELECTRICITY_METER: {
      publishHADiscoveryEM(element);
      break;
    }
    case SUPLA_CHANNELTYPE_DIMMER: {
      publishHADiscoveryDimmer(element);
      break;
    }
    case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER: {
      publishHADiscoveryRGB(element);
      break;
    }
    case SUPLA_CHANNELTYPE_DIMMERANDRGBLED: {
      publishHADiscoveryDimmer(element);
      publishHADiscoveryRGB(element);
      break;
    }
    case SUPLA_CHANNELTYPE_HVAC: {
      publishHADiscoveryHVAC(element);
      break;
    }
    // TODO(klew): add more channels here
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

void Supla::Protocol::Mqtt::publishHADiscoveryEMParameter(
    Supla::Element *element, int parameterId, const char *parameterName,
    const char *units, Supla::Protocol::HAStateClass stateClass,
    Supla::Protocol::HADeviceClass deviceClass) {
  if (element == nullptr) {
    return;
  }
  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  int phase = (parameterId + 12 - 5) / 12;

  char phaseStr[11] = {};  // " - Phase x" - 10 chars + null byte
  if (phase > 0 && phase < 4) {
    snprintf(phaseStr, sizeof(phaseStr), " - Phase %d", phase);
  }

  char humanReadableParameterName[200] = {};
  snprintf(humanReadableParameterName, sizeof(humanReadableParameterName),
      "%s%s", parameterName, phaseStr);
  humanReadableParameterName[0] -= 32;  // capitalize first char in name
  for (int i = 0; humanReadableParameterName[i] != 0; i++) {
    if (humanReadableParameterName[i] == '_') {
      humanReadableParameterName[i] = ' ';
    }
  }

  char phaseTopicPart[10] = {};  // "phases/x/" - 9 chars + null byte
  if (phase > 0 && phase < 4) {
    snprintf(phaseTopicPart, sizeof(phaseTopicPart), "phases/%d/", phase);
  }

  char objectId[30] = {};
  generateObjectId(objectId, element->getChannelNumber(), parameterId);

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
      "\"name\":\"#%i Electricity Meter (%s)\","
      "\"uniq_id\":\"supla_%s\","
      "\"qos\":0,"
      "\"unit_of_meas\": \"%s\","
      "\"stat_t\":\"~/state/%s%s\""
      "%s%s"
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
            humanReadableParameterName,
            objectId,
            units,
            phaseTopicPart,
            parameterName,
            getStateClassStr(stateClass),
            getDeviceClassStr(deviceClass)
            )
        + 1;

    if (i == 0) {
      payload = new char[bufferSize];
      if (payload == nullptr) {
        return;
      }
    }
  }

  publish(topic.c_str(), payload, -1, -1, true);

  delete[] payload;
}

void Supla::Protocol::Mqtt::publishHADiscoveryEM(Supla::Element *element) {
  if (element == nullptr) {
    return;
  }

  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  TElectricityMeter_ExtendedValue_V2 extEMValue = {};

  if (!ch->getExtValueAsElectricityMeter(&extEMValue)) {
    SUPLA_LOG_DEBUG("Mqtt: failed to obtain ext EM value");
    return;
  }

  int parameterId = 1;
  if (ElectricityMeter::isFwdActEnergyUsed(extEMValue)) {
    publishHADiscoveryEMParameter(element, parameterId,
        "total_forward_active_energy", "kWh",
        Supla::Protocol::HAStateClass_TotalIncreasing,
        Supla::Protocol::HADeviceClass_Energy);
  }

  parameterId++;
  if (ElectricityMeter::isRvrActEnergyUsed(extEMValue)) {
    publishHADiscoveryEMParameter(element, parameterId,
        "total_reverse_active_energy", "kWh",
        Supla::Protocol::HAStateClass_TotalIncreasing,
        Supla::Protocol::HADeviceClass_Energy);
  }

  parameterId += 2;  // we add 2 here, because it is left for meters with
                     // balanced energy values (not yet implemented here)

  for (int phase = 0; phase < MAX_PHASES; phase++) {
    if ((phase == 0 &&
          ch->getFlags() & SUPLA_CHANNEL_FLAG_PHASE1_UNSUPPORTED) ||
        (phase == 1 &&
         ch->getFlags() & SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED) ||
        (phase == 2 &&
         ch->getFlags() & SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED)
       ) {
      SUPLA_LOG_DEBUG("Mqtt: phase %d disabled, skipping", phase);
      parameterId += 12;
      continue;
    }

    parameterId++;
    if (ElectricityMeter::isFwdActEnergyUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "total_forward_active_energy", "kWh",
          Supla::Protocol::HAStateClass_TotalIncreasing,
          Supla::Protocol::HADeviceClass_Energy);
    }

    parameterId++;
    if (ElectricityMeter::isRvrActEnergyUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "total_reverse_active_energy", "kWh",
          Supla::Protocol::HAStateClass_TotalIncreasing,
          Supla::Protocol::HADeviceClass_Energy);
    }

    parameterId++;
    if (ElectricityMeter::isFwdReactEnergyUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "total_forward_reactive_energy", "kvarh",
          Supla::Protocol::HAStateClass_TotalIncreasing,
          Supla::Protocol::HADeviceClass_Energy);
    }

    parameterId++;
    if (ElectricityMeter::isRvrReactEnergyUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "total_reverse_reactive_energy", "kvarh",
          Supla::Protocol::HAStateClass_TotalIncreasing,
          Supla::Protocol::HADeviceClass_Energy);
    }

    parameterId++;
    if (ElectricityMeter::isFreqUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "frequency", "Hz",
          Supla::Protocol::HAStateClass_Measurement,
          Supla::Protocol::HADeviceClass_Frequency);
    }

    parameterId++;
    if (ElectricityMeter::isVoltageUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "voltage", "V",
          Supla::Protocol::HAStateClass_Measurement,
          Supla::Protocol::HADeviceClass_Voltage);
    }

    parameterId++;
    if (ElectricityMeter::isCurrentUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "current", "A",
          Supla::Protocol::HAStateClass_Measurement,
          Supla::Protocol::HADeviceClass_Current);
    }

    parameterId++;
    if (ElectricityMeter::isPowerActiveUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "power_active", "W",
          Supla::Protocol::HAStateClass_Measurement,
          Supla::Protocol::HADeviceClass_Power);
    }

    parameterId++;
    if (ElectricityMeter::isPowerReactiveUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "power_reactive", "var",
          Supla::Protocol::HAStateClass_Measurement,
          Supla::Protocol::HADeviceClass_ReactivePower);
    }

    parameterId++;
    if (ElectricityMeter::isPowerApparentUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "power_apparent", "VA",
          Supla::Protocol::HAStateClass_Measurement,
          Supla::Protocol::HADeviceClass_ApparentPower);
    }

    parameterId++;
    if (ElectricityMeter::isPowerFactorUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "power_factor", "",
          Supla::Protocol::HAStateClass_Measurement,
          Supla::Protocol::HADeviceClass_PowerFactor);
    }

    parameterId++;
    if (ElectricityMeter::isPhaseAngleUsed(extEMValue)) {
      publishHADiscoveryEMParameter(element, parameterId,
          "phase_angle", "°",
          Supla::Protocol::HAStateClass_Measurement,
          Supla::Protocol::HADeviceClass_None);
    }
  }
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

void Supla::Protocol::Mqtt::sendExtendedChannelValueChanged(
    uint8_t channelNumber,
    TSuplaChannelExtendedValue *value) {
  (void)(value);
  if (!isRegisteredAndReady()) {
    return;
  }

  publishExtendedChannelState(channelNumber);
}

const char *Supla::Protocol::Mqtt::getStateClassStr(
    Supla::Protocol::HAStateClass stateClass) {
  switch (stateClass) {
    case  HAStateClass_Measurement:
      return ",\"stat_cla\":\"measurement\"";
    case HAStateClass_Total:
      return ",\"stat_cla\":\"total\"";
    case HAStateClass_TotalIncreasing:
      return ",\"stat_cla\":\"total_increasing\"";
    case HAStateClass_None:
    default:
      return "";
  }
}

const char *Supla::Protocol::Mqtt::getDeviceClassStr(
    Supla::Protocol::HADeviceClass deviceClass) {
  switch (deviceClass) {
    case  HADeviceClass_Energy:
      return ",\"dev_cla\":\"energy\"";
    case HADeviceClass_ApparentPower:
      return ",\"dev_cla\":\"apparent_power\"";
    case HADeviceClass_Voltage:
      return ",\"dev_cla\":\"voltage\"";
    case HADeviceClass_Current:
      return ",\"dev_cla\":\"current\"";
    case HADeviceClass_Frequency:
      return ",\"dev_cla\":\"frequency\"";
    case HADeviceClass_PowerFactor:
      return ",\"dev_cla\":\"power_factor\"";
    case HADeviceClass_Power:
      return ",\"dev_cla\":\"power\"";
    case HADeviceClass_ReactivePower:
      return ",\"dev_cla\":\"reactive_power\"";
    case HADeviceClass_None:
    default:
      return "";
  }
}

void Mqtt::publishHADiscoveryRGB(Supla::Element *element) {
  if (element == nullptr) {
    return;
  }

  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  char objectId[30] = {};
  int subId = ch->getChannelType() == SUPLA_CHANNELTYPE_DIMMERANDRGBLED ?
    1 : 0;
  generateObjectId(objectId, element->getChannelNumber(), subId);

  auto topic = getHADiscoveryTopic("light", objectId);

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
      "\"name\":\"RGB Lighting\","
      "\"uniq_id\":\"supla_%s\","
      "\"qos\":0,"
      "\"ret\":false,"
      "\"opt\":false,"
      "\"stat_t\":\"~/state%s/on\","
      "\"cmd_t\":\"~/execute_action%s\","
      "\"pl_on\":\"TURN_ON\","
      "\"pl_off\":\"TURN_OFF\","
      "\"stat_val_tpl\":\"{%% if value == \\\"true\\\" %%}TURN_ON{%% else "
        "%%}TURN_OFF{%% endif %%}\","
      "\"on_cmd_type\":\"last\","
      "\"bri_cmd_t\":\"~/set/color_brightness\","
      "\"bri_scl\":100,"
      "\"bri_stat_t\":\"~/state/color_brightness\","
      "\"rgb_stat_t\":\"~/state/color\","
      "\"rgb_cmd_t\":\"~/set/color\""
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
            objectId,
            subId == 1 ? "/rgb" : "",
            subId == 1 ? "/rgb" : ""
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

void Mqtt::publishHADiscoveryDimmer(Supla::Element *element) {
  if (element == nullptr) {
    return;
  }

  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  bool subchannels = ch->getChannelType() == SUPLA_CHANNELTYPE_DIMMERANDRGBLED;
  char objectId[30] = {};
  generateObjectId(objectId, element->getChannelNumber(), 0);

  auto topic = getHADiscoveryTopic("light", objectId);

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
      "\"name\":\"Dimmer\","
      "\"uniq_id\":\"supla_%s\","
      "\"qos\":0,"
      "\"ret\":false,"
      "\"opt\":false,"
      "\"stat_t\":\"~/state%s/on\","
      "\"cmd_t\":\"~/execute_action%s\","
      "\"pl_on\":\"TURN_ON\","
      "\"pl_off\":\"TURN_OFF\","
      "\"stat_val_tpl\":\"{%% if value == \\\"true\\\" %%}TURN_ON{%% else "
        "%%}TURN_OFF{%% endif %%}\","
      "\"on_cmd_type\":\"last\","
      "\"bri_cmd_t\":\"~/set/brightness\","
      "\"bri_scl\":100,"
      "\"bri_stat_t\":\"~/state/brightness\""
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
            objectId,
            subchannels ? "/dimmer" : "",
            subchannels ? "/dimmer" : ""
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

void Mqtt::processRelayRequest(const char *part,
                               const char *payload,
                               Supla::Element *element) {
  TSD_SuplaChannelNewValue newValue = {};
  element->fillSuplaChannelNewValue(&newValue);

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
      newValue.value[0] = element->getChannel()->getValueBool() ? 0 : 1;
      element->handleNewValueFromServer(&newValue);
    } else {
      SUPLA_LOG_DEBUG("Mqtt: unsupported action %s", payload);
    }
  } else {
    SUPLA_LOG_DEBUG("Mqtt: received unsupported topic %s", part);
  }
}

void Mqtt::processRGBWRequest(const char *part,
                              const char *payload,
                              Supla::Element *element) {
  TSD_SuplaChannelNewValue newValue = {};
  element->fillSuplaChannelNewValue(&newValue);

  if (strcmp(part, "set/color_brightness") == 0) {
    int brightness = stringToInt(payload);
    if (brightness >= 0 && brightness <= 100) {
      newValue.value[1] = brightness;
      newValue.value[6] = RGBW_COMMAND_SET_COLOR_BRIGHTNESS_WITHOUT_TURN_ON;
      element->handleNewValueFromServer(&newValue);
    }
  } else if (strcmp(part, "set/brightness") == 0) {
    int brightness = stringToInt(payload);
    if (brightness >= 0 && brightness <= 100) {
      newValue.value[0] = brightness;
      newValue.value[6] = RGBW_COMMAND_SET_BRIGHTNESS_WITHOUT_TURN_ON;
      element->handleNewValueFromServer(&newValue);
    }
  } else if (strcmp(part, "execute_action/rgb") == 0) {
    if (strncmpInsensitive(payload, "turn_on", 8) == 0) {
      newValue.value[6] = RGBW_COMMAND_TURN_ON_RGB;
    } else if (strncmpInsensitive(payload, "turn_off", 9) == 0) {
      newValue.value[6] = RGBW_COMMAND_TURN_OFF_RGB;
    } else if (strncmpInsensitive(payload, "toggle", 7) == 0) {
      newValue.value[6] = RGBW_COMMAND_TOGGLE_RGB;
    } else {
      SUPLA_LOG_DEBUG("Mqtt: unsupported action %s", payload);
      return;
    }
    element->handleNewValueFromServer(&newValue);
  } else if (strcmp(part, "execute_action/dimmer") == 0) {
    if (strncmpInsensitive(payload, "turn_on", 8) == 0) {
      newValue.value[6] = RGBW_COMMAND_TURN_ON_DIMMER;
    } else if (strncmpInsensitive(payload, "turn_off", 9) == 0) {
      newValue.value[6] = RGBW_COMMAND_TURN_OFF_DIMMER;
    } else if (strncmpInsensitive(payload, "toggle", 7) == 0) {
      newValue.value[6] = RGBW_COMMAND_TOGGLE_DIMMER;
    } else {
      SUPLA_LOG_DEBUG("Mqtt: unsupported action %s", payload);
      return;
    }
    element->handleNewValueFromServer(&newValue);
  } else if (strcmp(part, "set/color") == 0) {
    SUPLA_LOG_DEBUG("PAYLOAD %s", payload);
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    if (stringToColor(payload, &red, &green, &blue)) {
      newValue.value[2] = blue;
      newValue.value[3] = green;
      newValue.value[4] = red;
      newValue.value[6] = RGBW_COMMAND_SET_RGB_WITHOUT_TURN_ON;
      element->handleNewValueFromServer(&newValue);
    }
  }
}

void Mqtt::processRGBRequest(const char *part,
                             const char *payload,
                             Supla::Element *element) {
  TSD_SuplaChannelNewValue newValue = {};
  element->fillSuplaChannelNewValue(&newValue);

  if (strcmp(part, "set/color_brightness") == 0) {
    int brightness = stringToInt(payload);
    if (brightness >= 0 && brightness <= 100) {
      newValue.value[1] = brightness;
      newValue.value[6] = RGBW_COMMAND_SET_COLOR_BRIGHTNESS_WITHOUT_TURN_ON;
      element->handleNewValueFromServer(&newValue);
    }
  } else if (strcmp(part, "execute_action") == 0) {
    if (strncmpInsensitive(payload, "turn_on", 8) == 0) {
      newValue.value[6] = RGBW_COMMAND_TURN_ON_RGB;
    } else if (strncmpInsensitive(payload, "turn_off", 9) == 0) {
      newValue.value[6] = RGBW_COMMAND_TURN_OFF_RGB;
    } else if (strncmpInsensitive(payload, "toggle", 7) == 0) {
      newValue.value[6] = RGBW_COMMAND_TOGGLE_RGB;
    } else {
      SUPLA_LOG_DEBUG("Mqtt: unsupported action %s", payload);
      return;
    }
    element->handleNewValueFromServer(&newValue);
  } else if (strcmp(part, "set/color") == 0) {
    SUPLA_LOG_DEBUG("PAYLOAD %s", payload);
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    if (stringToColor(payload, &red, &green, &blue)) {
      newValue.value[2] = blue;
      newValue.value[3] = green;
      newValue.value[4] = red;
      newValue.value[6] = RGBW_COMMAND_SET_RGB_WITHOUT_TURN_ON;
      element->handleNewValueFromServer(&newValue);
    }
  }
}

void Mqtt::processDimmerRequest(const char *part,
                                const char *payload,
                                Supla::Element *element) {
  TSD_SuplaChannelNewValue newValue = {};
  element->fillSuplaChannelNewValue(&newValue);

  if (strcmp(part, "set/brightness") == 0) {
    int brightness = stringToInt(payload);
    if (brightness >= 0 && brightness <= 100) {
      newValue.value[0] = brightness;
      newValue.value[6] = RGBW_COMMAND_SET_BRIGHTNESS_WITHOUT_TURN_ON;
      element->handleNewValueFromServer(&newValue);
    }
  } else if (strcmp(part, "execute_action") == 0) {
    if (strncmpInsensitive(payload, "turn_on", 8) == 0) {
      newValue.value[6] = RGBW_COMMAND_TURN_ON_DIMMER;
      element->handleNewValueFromServer(&newValue);
    } else if (strncmpInsensitive(payload, "turn_off", 9) == 0) {
      newValue.value[6] = RGBW_COMMAND_TURN_OFF_DIMMER;
      element->handleNewValueFromServer(&newValue);
    } else if (strncmpInsensitive(payload, "toggle", 7) == 0) {
      newValue.value[6] = RGBW_COMMAND_TOGGLE_DIMMER;
    } else {
      SUPLA_LOG_DEBUG("Mqtt: unsupported action %s", payload);
      return;
    }
    element->handleNewValueFromServer(&newValue);
  }
}

void Mqtt::publishHADiscoveryHVAC(Supla::Element *element) {
  if (element == nullptr) {
    return;
  }

  auto hvac = reinterpret_cast<Supla::Control::HvacBase *>(element);

  auto ch = element->getChannel();
  if (ch == nullptr) {
    return;
  }

  char objectId[30] = {};
  generateObjectId(objectId, element->getChannelNumber(), 0);

  // generate topics for related thermometer and hygrometer channels
  char temperatureTopic[100] = "None";
  char humidityTopic[100] = "None";
  auto tempChannelNo = hvac->getMainThermometerChannelNo();
  if (tempChannelNo != element->getChannelNumber()) {
    snprintf(temperatureTopic,
             sizeof(temperatureTopic),
             "%s/channels/%i/state/temperature",
             prefix, tempChannelNo);

    auto thermometerEl =
        Supla::Element::getElementByChannelNumber(tempChannelNo);
    if (thermometerEl != nullptr &&
        thermometerEl->getChannel()->getChannelType() ==
            SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) {
      snprintf(humidityTopic,
          sizeof(humidityTopic),
          "%s/channels/%i/state/humidity",
          prefix, tempChannelNo);
    }
  }

  auto topic = getHADiscoveryTopic("climate", objectId);
  int16_t tempMin = hvac->getTemperatureRoomMin();
  int16_t tempMax = hvac->getTemperatureRoomMax();

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
      "\"name\":\"#%i Thermostat\","
      "\"uniq_id\":\"supla_%s\","
      "\"qos\":0,"
      "\"ret\":false,"
      "\"opt\":false,"
      "\"action_topic\":\"~/state/action\","   // off, heating, cooling, drying,
                                               // idle, fan.
      "\"current_temperature_topic\":\"%s\","  // link to temperature sensor
      "\"current_humidity_topic\":\"%s\","  // link to humidity sensor
      "\"max_temp\":\"%.2f\","
      "\"min_temp\":\"%.2f\","
      "\"modes\":["
        "\"off\","
        "\"auto\","  // auto == weekly schedule
        "%s"  // remaining supported modes (depends on function)
        "],"
      "\"mode_stat_t\":\"~/state/mode\","
      "\"mode_command_topic\":\"~/execute_action\","
      "\"power_command_topic\":\"~/execute_action\","
      "\"payload_off\":\"turn_off\","
      "\"payload_on\":\"turn_on\","
      "\"temperature_unit\":\"C\","
      "\"temp_step\":\"0.1\","
      "%s"  // tempearture setpoints
      "}";

  char c = '\0';

  size_t bufferSize = 0;
  char *payload = {};

  for (int i = 0; i < 2; i++) {
    bufferSize =
        snprintf(
            i ? payload : &c,
            i ? bufferSize : 1,
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
            temperatureTopic,
            humidityTopic,
            static_cast<double>(tempMax) / 100.0,
            static_cast<double>(tempMin) / 100.0,
            (hvac->getChannelFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO
                 ? "\"heat\",\"cool\",\"heat_cool\""
                 :
                 (hvac->isCoolingSubfunction()
                  ? "\"cool\"" : "\"heat\"")),
            (hvac->getChannelFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO
                 ? "\"temperature_high_command_topic\":\""
                   "~/set/temperature_setpoint_cool/\","
                   "\"temperature_high_state_topic\":\""
                   "~/state/temperature_setpoint_cool/\","
                   "\"temperature_low_command_topic\":\""
                   "~/set/temperature_setpoint_heat/\","
                   "\"temperature_low_state_topic\":\""
                   "~/state/temperature_setpoint_heat/\""
                 : "\"temperature_command_topic\":\""
                   "~/set/temperature_setpoint\","
                   "\"temperature_state_topic\":\""
                   "~/state/temperature_setpoint\"")) +
        1;

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

void Mqtt::processHVACRequest(const char *topic,
                              const char *payload,
                              Supla::Element *element) {
  TSD_SuplaChannelNewValue newValue = {};
  element->fillSuplaChannelNewValue(&newValue);
  THVACValue *hvacValue = reinterpret_cast<THVACValue *>(newValue.value);

  if (strcmp(topic, "set/temperature_setpoint_heat") == 0) {
    int32_t value = floatStringToInt(payload, 2);
    if (value < INT16_MIN || value > INT16_MAX) {
      return;
    }
    hvacValue->SetpointTemperatureHeat = value;
    hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
    element->handleNewValueFromServer(&newValue);
  } else if (strcmp(topic, "set/temperature_setpoint_cool") == 0) {
    int32_t value = floatStringToInt(payload, 2);
    if (value < INT16_MIN || value > INT16_MAX) {
      return;
    }
    hvacValue->SetpointTemperatureCool = value;
    hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
    element->handleNewValueFromServer(&newValue);
  } else if (strcmp(topic, "set/temperature_setpoint") == 0) {
    int32_t value = floatStringToInt(payload, 2);
    if (value < INT16_MIN || value > INT16_MAX) {
      return;
    }
    if (element && element->getChannel() &&
        element->getChannel()->getHvacFlagCoolSubfunction() ==
            HvacCoolSubfunctionFlag::CoolSubfunction) {
      hvacValue->SetpointTemperatureCool = value;
      hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
      element->handleNewValueFromServer(&newValue);
      return;
    }
    hvacValue->SetpointTemperatureHeat = value;
    hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
    element->handleNewValueFromServer(&newValue);
  } else if (strcmp(topic, "execute_action") == 0) {
    if (strncmpInsensitive(payload, "turn_on", 8) == 0) {
      hvacValue->Mode = SUPLA_HVAC_MODE_CMD_TURN_ON;
      element->handleNewValueFromServer(&newValue);
    } else if (strncmpInsensitive(payload, "turn_off", 9) == 0 ||
        strncmpInsensitive(payload, "off", 4) == 0) {
      hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
      element->handleNewValueFromServer(&newValue);
    } else if (strncmpInsensitive(payload, "toggle", 7) == 0) {
      if (element && element->getChannel() &&
          element->getChannel()->getHvacIsOn() != 0) {
        hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
      } else {
        hvacValue->Mode = SUPLA_HVAC_MODE_CMD_TURN_ON;
      }
      element->handleNewValueFromServer(&newValue);
    } else if (strncmpInsensitive(payload, "auto", 5) == 0) {
      hvacValue->Mode = SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE;
      element->handleNewValueFromServer(&newValue);
    } else if (strncmpInsensitive(payload, "heat", 5) == 0) {
      hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
      element->handleNewValueFromServer(&newValue);
    } else if (strncmpInsensitive(payload, "cool", 5) == 0) {
      hvacValue->Mode = SUPLA_HVAC_MODE_COOL;
      element->handleNewValueFromServer(&newValue);
    } else if (strncmpInsensitive(payload, "heat_cool", 10) == 0) {
      hvacValue->Mode = SUPLA_HVAC_MODE_AUTO;
      element->handleNewValueFromServer(&newValue);
    } else {
      SUPLA_LOG_DEBUG("Mqtt: unsupported action %s", payload);
    }
  } else {
    SUPLA_LOG_DEBUG("Mqtt: received unsupported topic %s", topic);
  }
}

void Mqtt::notifyConfigChange(int channelNumber) {
  if (channelNumber >= 0 && channelNumber < 255) {
    // set bit on configChangedBit[8]:
    configChangedBit[channelNumber / 8] |= (1 << (channelNumber % 8));
  }
}
