// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hvac_mqtt.h"

#include <supla/action_handler.h>
#include <supla/channel.h>
#include <supla/control/hvac_base.h>
#include <supla/device/register_device.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/mqtt.h>
#include <supla/protocol/mqtt_handler_registry.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/thermometer.h>
#include <supla/tools.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

namespace Supla {
namespace Protocol {

class HvacMqttHandler : public MqttChannelHandler {
 public:
  int mqttHandledChannelType() const override {
    return SUPLA_CHANNELTYPE_HVAC;
  }

  void mqttPublishChannelState(Mqtt *mqtt, Supla::Element *element) override {
    if (mqtt == nullptr || element == nullptr) {
      return;
    }

    auto ch = element->getChannel();
    if (ch == nullptr) {
      return;
    }

    auto topic = MqttTopic("channels") / element->getChannelNumber() / "state";

    if (ch->isHvacFlagHeating()) {
      mqtt->publish((topic / "action").c_str(), "heating", -1, 1);
    } else if (ch->isHvacFlagCooling()) {
      mqtt->publish((topic / "action").c_str(), "cooling", -1, 1);
    } else if (ch->getHvacMode() == SUPLA_HVAC_MODE_OFF ||
               ch->getHvacMode() == SUPLA_HVAC_MODE_NOT_SET) {
      mqtt->publish((topic / "action").c_str(), "off", -1, 1);
    } else {
      mqtt->publish((topic / "action").c_str(), "idle", -1, 1);
    }

    if (ch->isHvacFlagWeeklySchedule()) {
      mqtt->publish((topic / "mode").c_str(), "auto", -1, 1);
    } else {
      switch (ch->getHvacMode()) {
        case SUPLA_HVAC_MODE_HEAT: {
          mqtt->publish((topic / "mode").c_str(), "heat", -1, 1);
          break;
        }
        case SUPLA_HVAC_MODE_COOL: {
          mqtt->publish((topic / "mode").c_str(), "cool", -1, 1);
          break;
        }
        case SUPLA_HVAC_MODE_HEAT_COOL: {
          mqtt->publish((topic / "mode").c_str(), "heat_cool", -1, 1);
          break;
        }
        case SUPLA_HVAC_MODE_OFF:
        case SUPLA_HVAC_MODE_NOT_SET:
        default: {
          mqtt->publish((topic / "mode").c_str(), "off", -1, 1);
          break;
        }
      }
    }

    auto hvac = reinterpret_cast<Supla::Control::HvacBase *>(element);
    if (hvac->getChannelFunction() ==
        SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL) {
      mqtt->publish((topic / "temperature_setpoint").c_str(), "", -1, 1);
      int16_t setpointHeat = ch->getHvacSetpointTemperatureHeat();
      if (setpointHeat > INT16_MIN) {
        mqtt->publishDouble((topic / "temperature_setpoint_heat").c_str(),
                            static_cast<double>(setpointHeat) / 100.0,
                            -1,
                            1,
                            2);
      } else {
        mqtt->publish((topic / "temperature_setpoint_heat").c_str(), "", -1, 1);
      }

      int16_t setpointCool = ch->getHvacSetpointTemperatureCool();
      if (setpointCool > INT16_MIN) {
        mqtt->publishDouble((topic / "temperature_setpoint_cool").c_str(),
                            static_cast<double>(setpointCool) / 100.0,
                            -1,
                            1,
                            2);
      } else {
        mqtt->publish((topic / "temperature_setpoint_cool").c_str(), "", -1, 1);
      }
    } else {
      int16_t temperatureSetpoint = ch->getHvacSetpointTemperatureHeat();
      if (ch->getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
          ch->getHvacFlagCoolSubfunction() ==
              HvacCoolSubfunctionFlag::CoolSubfunction) {
        temperatureSetpoint = ch->getHvacSetpointTemperatureCool();
      }
      mqtt->publish((topic / "temperature_setpoint_heat").c_str(), "", -1, 1);
      mqtt->publish((topic / "temperature_setpoint_cool").c_str(), "", -1, 1);
      mqtt->publishDouble((topic / "temperature_setpoint").c_str(),
                          static_cast<double>(temperatureSetpoint) / 100.0,
                          -1,
                          1,
                          2);
    }
  }

  void mqttSubscribeChannel(Mqtt *mqtt, Supla::Element *element) override {
    if (mqtt == nullptr || element == nullptr) {
      return;
    }
    auto topic = MqttTopic("channels") / element->getChannelNumber();
    mqtt->subscribe((topic / "execute_action").c_str());
    mqtt->subscribe((topic / "set" / "temperature_setpoint").c_str());
    mqtt->subscribe((topic / "set" / "temperature_setpoint_heat").c_str());
    mqtt->subscribe((topic / "set" / "temperature_setpoint_cool").c_str());
  }

  bool mqttProcessData(Mqtt *mqtt,
                       const char *topic_part,
                       const char *payload,
                       Supla::Element *element) override {
    (void)(mqtt);
    if (topic_part == nullptr || payload == nullptr || element == nullptr) {
      return false;
    }

    TSD_SuplaChannelNewValue newValue = {};
    element->fillSuplaChannelNewValue(&newValue);
    THVACValue *hvacValue = reinterpret_cast<THVACValue *>(newValue.value);

    if (strcmp(topic_part, "set/temperature_setpoint_heat") == 0) {
      int32_t value = floatStringToInt(payload, 2);
      if (value < INT16_MIN || value > INT16_MAX) {
        return false;
      }
      hvacValue->SetpointTemperatureHeat = value;
      hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
      element->handleNewValueFromServer(&newValue);
      return true;
    }

    if (strcmp(topic_part, "set/temperature_setpoint_cool") == 0) {
      int32_t value = floatStringToInt(payload, 2);
      if (value < INT16_MIN || value > INT16_MAX) {
        return false;
      }
      hvacValue->SetpointTemperatureCool = value;
      hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
      element->handleNewValueFromServer(&newValue);
      return true;
    }

    if (strcmp(topic_part, "set/temperature_setpoint") == 0) {
      int32_t value = floatStringToInt(payload, 2);
      if (value < INT16_MIN || value > INT16_MAX) {
        return false;
      }
      if (element->getChannel() &&
          element->getChannel()->getHvacFlagCoolSubfunction() ==
              HvacCoolSubfunctionFlag::CoolSubfunction) {
        hvacValue->SetpointTemperatureCool = value;
        hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
      } else {
        hvacValue->SetpointTemperatureHeat = value;
        hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
      }
      element->handleNewValueFromServer(&newValue);
      return true;
    }

    if (strcmp(topic_part, "execute_action") == 0) {
      if (strncmpInsensitive(payload, "turn_on", 8) == 0) {
        hvacValue->Mode = SUPLA_HVAC_MODE_CMD_TURN_ON;
      } else if (strncmpInsensitive(payload, "turn_off", 9) == 0 ||
                 strncmpInsensitive(payload, "off", 4) == 0) {
        hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
      } else if (strncmpInsensitive(payload, "toggle", 7) == 0) {
        if (element->getChannel() &&
            element->getChannel()->getHvacIsOnRaw() != 0) {
          hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
        } else {
          hvacValue->Mode = SUPLA_HVAC_MODE_CMD_TURN_ON;
        }
      } else if (strncmpInsensitive(payload, "auto", 5) == 0) {
        hvacValue->Mode = SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE;
      } else if (strncmpInsensitive(payload, "heat", 5) == 0) {
        hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
      } else if (strncmpInsensitive(payload, "cool", 5) == 0) {
        hvacValue->Mode = SUPLA_HVAC_MODE_COOL;
      } else if (strncmpInsensitive(payload, "heat_cool", 10) == 0) {
        hvacValue->Mode = SUPLA_HVAC_MODE_HEAT_COOL;
      } else {
        SUPLA_LOG_DEBUG("Mqtt: unsupported action %s", payload);
        return false;
      }
      element->handleNewValueFromServer(&newValue);
      return true;
    }

    return false;
  }

  void mqttPublishHADiscovery(Mqtt *mqtt, Supla::Element *element) override {
    if (mqtt == nullptr || element == nullptr) {
      return;
    }

    auto hvac = reinterpret_cast<Supla::Control::HvacBase *>(element);
    auto ch = element->getChannel();
    if (ch == nullptr) {
      return;
    }

    char objectId[30] = {};
    mqtt->generateObjectId(objectId, element->getChannelNumber(), 0);

    char temperatureTopic[100] = "None";
    char humidityTopic[100] = "None";
    auto tempChannelNo = hvac->getMainThermometerChannelNo();
    if (tempChannelNo != element->getChannelNumber()) {
      snprintf(temperatureTopic,
               sizeof(temperatureTopic),
               "%s/channels/%i/state/temperature",
               mqtt->getPrefix(),
               tempChannelNo);

      auto thermometerEl =
          Supla::Element::getElementByChannelNumber(tempChannelNo);
      if (thermometerEl != nullptr &&
          thermometerEl->getChannel()->getChannelType() ==
              SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) {
        snprintf(humidityTopic,
                 sizeof(humidityTopic),
                 "%s/channels/%i/state/humidity",
                 mqtt->getPrefix(),
                 tempChannelNo);
      }
    }

    auto topic = mqtt->getHADiscoveryTopic("climate", objectId);
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
        "\"action_topic\":\"~/state/action\","
        "\"current_temperature_topic\":\"%s\","
        "\"current_humidity_topic\":\"%s\","
        "\"max_temp\":\"%.2f\","
        "\"min_temp\":\"%.2f\","
        "\"modes\":["
        "\"off\","
        "\"auto\","
        "%s"
        "],"
        "\"mode_stat_t\":\"~/state/mode\","
        "\"mode_command_topic\":\"~/execute_action\","
        "\"power_command_topic\":\"~/execute_action\","
        "\"payload_off\":\"turn_off\","
        "\"payload_on\":\"turn_on\","
        "\"temperature_unit\":\"C\","
        "\"temp_step\":\"0.1\","
        "%s"
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
              mqtt->getPrefix(),
              mqtt->getPrefix(),
              ch->getChannelNumber(),
              mqtt->getHostname(),
              getManufacturer(Supla::RegisterDevice::getManufacturerId()),
              Supla::RegisterDevice::getName(),
              Supla::RegisterDevice::getSoftVer(),
              element->getChannelNumber(),
              objectId,
              temperatureTopic,
              humidityTopic,
              static_cast<double>(tempMax) / 100.0,
              static_cast<double>(tempMin) / 100.0,
              (hvac->getChannelFunction() ==
                       SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL
                   ? "\"heat\",\"cool\",\"heat_cool\""
                   : (hvac->isCoolingSubfunction() ? "\"cool\"" : "\"heat\"")),
              (hvac->getChannelFunction() ==
                       SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL
                   ? "\"temperature_high_command_topic\":\"~/set/"
                     "temperature_setpoint_cool/\","
                     "\"temperature_high_state_topic\":\"~/state/"
                     "temperature_setpoint_cool/\","
                     "\"temperature_low_command_topic\":\"~/set/"
                     "temperature_setpoint_heat/\","
                     "\"temperature_low_state_topic\":\"~/state/"
                     "temperature_setpoint_heat/\""
                   : "\"temperature_command_topic\":\"~/set/"
                     "temperature_setpoint\","
                     "\"temperature_state_topic\":\"~/state/"
                     "temperature_setpoint\"")) +
          1;
      if (i == 0) {
        payload = new char[bufferSize];
        if (payload == nullptr) {
          return;
        }
      }
    }

    mqtt->publish(topic.c_str(), payload, -1, 1, true);
    delete[] payload;
  }
};

void RegisterHvacMqttHandler() {
  static HvacMqttHandler handler;
  MqttHandlerRegistry::instance().registerHandler(&handler);
}

}  // namespace Protocol
}  // namespace Supla
