// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR

#include "impulse_counter_mqtt.h"

#include <SuplaDevice.h>
#include <stdio.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/device/register_device.h>
#include <supla/element.h>
#include <supla/protocol/mqtt.h>
#include <supla/protocol/mqtt_handler_registry.h>
#include <supla/protocol/mqtt_topic.h>
#include <supla/tools.h>

#include <new>

namespace Supla {
namespace Protocol {
namespace {

MqttTopic stateTopic(int channel) {
  return MqttTopic("channels") / channel / "state" / "value";
}

// Keep discovery literals out of data RAM on ESP8266.
#if defined(ARDUINO_ARCH_ESP8266)
#define IC_DISCOVERY_LITERAL(json, value) \
    (json).appendFlash(PSTR(value), sizeof(value) - 1)
#else
#define IC_DISCOVERY_LITERAL(json, value) (json).append(value)
#endif

// The same writer measures the payload and writes it without temporary strings.
class DiscoveryJson {
 public:
  explicit DiscoveryJson(char *output = nullptr, size_t capacity = 0)
      : output(output), capacity(capacity) {}

  size_t size() const {
    return length;
  }

  void append(const char *value) {
    append(value, strlen(value));
  }

#if defined(ARDUINO_ARCH_ESP8266)
  void appendFlash(const char *value, size_t size) {
    if (canWrite(size)) {
      memcpy_P(output + length, value, size);
    }
    length += size;
  }
#endif

  void escape(const char *value) {
    static const char hex[] = "0123456789abcdef";
    if (!value) {
      return;
    }
    for (; *value; value++) {
      const auto byte = static_cast<unsigned char>(*value);
      if (byte < 0x20) {
        const char sequence[] = {'\\', 'u', '0', '0',
                                  hex[byte >> 4], hex[byte & 0x0f]};
        append(sequence, sizeof(sequence));
      } else {
        if (byte == '"' || byte == '\\') {
          append("\\", 1);
        }
        append(value, 1);
      }
    }
  }

 private:
  bool canWrite(size_t size) const {
    return output && length <= capacity && size <= capacity - length;
  }

  void append(const char *value, size_t size) {
    if (canWrite(size)) {
      memcpy(output + length, value, size);
    }
    length += size;
  }

  char *output;
  size_t capacity;
  size_t length = 0;
};

}  // namespace

class ImpulseCounterMqttHandler : public MqttChannelHandler {
 public:
  int mqttHandledChannelType() const override {
    return SUPLA_CHANNELTYPE_IMPULSE_COUNTER;
  }

  void mqttPublishChannelState(Mqtt *mqtt, Element *element) override {
    auto channel = element->getChannel();
    auto topic = stateTopic(channel->getChannelNumber());
    double value = 0;
    if (channel->getDefaultFunction() != 0 &&
        channel->getImpulseCounterValue(&value, nullptr)) {
      mqtt->publishDouble(topic.c_str(), value, -1,
                          channel->isSleepingEnabled() ? 0 : 1, 3);
    } else {
      mqtt->publish(topic.c_str(), "", -1, 1);
    }
  }

  void mqttSubscribeChannel(Mqtt *, Element *) override {}
  void mqttUnsubscribeChannel(Mqtt *, Element *) override {}
  bool mqttProcessData(Mqtt *, const char *, const char *, Element *) override {
    return false;
  }

  void mqttClearChannelState(Mqtt *mqtt, Channel *channel) override {
    mqtt->publish(stateTopic(channel->getChannelNumber()).c_str(), "", -1, 1);
  }

  void mqttClearHADiscovery(Mqtt *mqtt, Channel *channel) override {
    char objectId[30] = {};
    mqtt->generateObjectId(objectId, channel->getChannelNumber(), 0);
    auto topic = mqtt->getHADiscoveryTopic("sensor", objectId);
    mqtt->publish(topic.c_str(), "", -1, 1, true);
  }

  void mqttPublishHADiscovery(Mqtt *mqtt, Element *element) override {
    auto channel = element->getChannel();
    const char *unit = nullptr;
    const auto function = channel->getDefaultFunction();
    if (function == 0 || !channel->getImpulseCounterValue(nullptr, &unit)) {
      mqttClearHADiscovery(mqtt, channel);
      return;
    }

    const char *deviceClass = "";
    if (!unit || !unit[0]) {
      unit = "";
      switch (function) {
        case SUPLA_CHANNELFNC_IC_ELECTRICITY_METER:
          deviceClass = "energy";
          unit = "kWh";
          break;
        case SUPLA_CHANNELFNC_IC_GAS_METER:
          deviceClass = "gas";
          unit = "m³";
          break;
        case SUPLA_CHANNELFNC_IC_WATER_METER:
          deviceClass = "water";
          unit = "m³";
          break;
        case SUPLA_CHANNELFNC_IC_HEAT_METER:
          deviceClass = "energy";
          unit = "GJ";
          break;
      }
    }

    const char *prefix = mqtt->getPrefix();
    if (!prefix) {
      return;
    }
    const int number = channel->getChannelNumber();
    char channelNumber[12] = {};
    snprintf(channelNumber, sizeof(channelNumber), "%d", number);
    char objectId[30] = {};
    mqtt->generateObjectId(objectId, number, 0);
    auto topic = mqtt->getHADiscoveryTopic("sensor", objectId);
    char expireAfter[40] = {};
    mqtt->getHAExpireAfter(channel, expireAfter, sizeof(expireAfter));
    const bool sleeping = mqtt->getSdc()->isSleepingDeviceEnabled();

    const auto writeDiscovery = [&](DiscoveryJson &json) {
      IC_DISCOVERY_LITERAL(json, "{\"avty\":[");
      if (!sleeping) {
        IC_DISCOVERY_LITERAL(json, "{\"t\":\"");
        json.escape(prefix);
        IC_DISCOVERY_LITERAL(json,
                             "/state/connected\",\"pl_avail\":\"true\","
                             "\"pl_not_avail\":\"false\"},");
      }
      IC_DISCOVERY_LITERAL(json, "{\"t\":\"");
      json.escape(prefix);
      IC_DISCOVERY_LITERAL(json, "/channels/");
      json.append(channelNumber);
      IC_DISCOVERY_LITERAL(json,
                           "/state/available\",\"pl_avail\":\"true\","
                           "\"pl_not_avail\":\"false\"}],");
      if (!sleeping) {
        IC_DISCOVERY_LITERAL(json, "\"avty_mode\":\"all\",");
      }
      json.append(expireAfter);
      IC_DISCOVERY_LITERAL(json, "\"~\":\"");
      json.escape(prefix);
      IC_DISCOVERY_LITERAL(json, "/channels/");
      json.append(channelNumber);
      IC_DISCOVERY_LITERAL(json, "\",\"dev\":{\"ids\":\"");
      json.escape(mqtt->getHostname());
      IC_DISCOVERY_LITERAL(json, "\",\"mf\":\"");
      json.escape(getManufacturer(RegisterDevice::getManufacturerId()));
      IC_DISCOVERY_LITERAL(json, "\",\"name\":\"");
      json.escape(RegisterDevice::getName());
      IC_DISCOVERY_LITERAL(json, "\",\"sw\":\"");
      json.escape(RegisterDevice::getSoftVer());
      IC_DISCOVERY_LITERAL(json, "\"},\"name\":\"#");
      json.append(channelNumber);
      IC_DISCOVERY_LITERAL(json, " Impulse counter\",\"uniq_id\":\"supla_");
      json.append(objectId);
      IC_DISCOVERY_LITERAL(json,
                           "\",\"stat_cla\":\"total_increasing\","
                           "\"stat_t\":\"~/state/value\"");
      if (*deviceClass) {
        IC_DISCOVERY_LITERAL(json, ",\"dev_cla\":\"");
        json.append(deviceClass);
        IC_DISCOVERY_LITERAL(json, "\"");
      }
      if (*unit) {
        IC_DISCOVERY_LITERAL(json, ",\"unit_of_meas\":\"");
        json.escape(unit);
        IC_DISCOVERY_LITERAL(json, "\"");
      }
      IC_DISCOVERY_LITERAL(json, "}");
    };

    DiscoveryJson measure;
    writeDiscovery(measure);
    const size_t size = measure.size();
    auto *payload = new (std::nothrow) char[size + 1];
    if (!payload) {
      return;
    }
    DiscoveryJson json(payload, size);
    writeDiscovery(json);
    if (json.size() <= size) {
      payload[json.size()] = '\0';
      mqtt->publish(topic.c_str(), payload, -1, 1, true);
    }
    delete[] payload;
  }
};

#undef IC_DISCOVERY_LITERAL

void RegisterImpulseCounterMqttHandler() {
  static ImpulseCounterMqttHandler handler;
  MqttHandlerRegistry::instance().registerHandler(&handler);
}

}  // namespace Protocol
}  // namespace Supla

#endif  // ARDUINO_ARCH_AVR
