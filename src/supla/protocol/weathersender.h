/*
 * Copyright (C) malarz
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

#ifndef SRC_SUPLA_PROTOCOL_WEATHERSENDER_H_
#define SRC_SUPLA_PROTOCOL_WEATHERSENDER_H_

#include <SuplaDevice.h>
#include <supla/tools.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/channel_element.h>

#define MAXSENSORS 12
namespace Supla {

enum SenorType : uint8_t {
  PM1,
  PM2_5,
  PM4,
  PM10,
  TEMP,
  HUMI,
  PRESS,
  LIGHT,
  WIND,
  RAIN,
  CO2,
};

namespace Protocol {

class WeatherSender : public Supla::Element {
 public:
  explicit WeatherSender(Supla::Network* _network) {
    network = _network;
    for (int i=0; i < MAXSENSORS; i++) {
      sensors[i] = nullptr;
    }
    lastSendTime = millis() - 100 * 1000;
  }

  void addSensor(Supla::SenorType type, Supla::LocalAction* sensor,
      int option = 0) {
    SUPLA_LOG_DEBUG("weathersender: added sensor [%d]", type);
    sensors[type] = sensor;
    options[type] = option;
  }

  double getSensorValue(int type) {
    double value = NAN;

    if (sensors[type] == nullptr) {
      return value;
    }

    switch (type) {
      case Supla::SenorType::PM1:
        value = ((Supla::Sensor::GeneralPurposeChannelBase*)(sensors[type]))
          ->getCalculatedValue();
        SUPLA_LOG_DEBUG("weathersender: pm1.0 = %.2f", value);
        break;
      case Supla::SenorType::PM2_5:
        value = ((Supla::Sensor::GeneralPurposeChannelBase*)(sensors[type]))
          ->getCalculatedValue();
        SUPLA_LOG_DEBUG("weathersender: pm2.5 = %.2f", value);
        break;
      case Supla::SenorType::PM4:
        value = ((Supla::Sensor::GeneralPurposeChannelBase*)(sensors[type]))
          ->getCalculatedValue();
        SUPLA_LOG_DEBUG("weathersender: pm4 = %.2f", value);
        break;
      case Supla::SenorType::PM10:
        value = ((Supla::Sensor::GeneralPurposeChannelBase*)(sensors[type]))
          ->getCalculatedValue();
        SUPLA_LOG_DEBUG("weathersender: pm10 = %.2f", value);
        break;
      case Supla::SenorType::TEMP:
        value = ((Supla::Channel*)(sensors[type]))
          ->getLastTemperature();
        SUPLA_LOG_DEBUG("weathersender: temperature = %.2f", value);
        break;
      case Supla::SenorType::HUMI:
        value = ((Supla::Channel*)(sensors[type]))
          ->getValueDoubleSecond();
        SUPLA_LOG_DEBUG("weathersender: humidity = %.2f", value);
        break;
      case Supla::SenorType::PRESS:
        value = ((Supla::Channel*)(sensors[type]))
          ->getValueDouble();
        SUPLA_LOG_DEBUG("weathersender: press = %.2f", value);
        break;
      case Supla::SenorType::LIGHT:
        value = ((Supla::Sensor::GeneralPurposeChannelBase*)(sensors[type]))
          ->getCalculatedValue();
        SUPLA_LOG_DEBUG("weathersender: light = %.2f", value);
        break;
      case Supla::SenorType::WIND:
        value = ((Supla::Channel*)(sensors[type]))
          ->getValueDouble();
        SUPLA_LOG_DEBUG("weathersender: wind = %.2f", value);
        break;
      case Supla::SenorType::RAIN:
        value = ((Supla::Channel*)(sensors[type]))
          ->getValueDouble();
        SUPLA_LOG_DEBUG("weathersender: rain = %.2f", value);
        break;
      case Supla::SenorType::CO2:
        value = ((Supla::Sensor::GeneralPurposeChannelBase*)(sensors[type]))
          ->getCalculatedValue();
        SUPLA_LOG_DEBUG("weathersender: co2 = %.2f", value);
        break;
    }

    return value;
  }

  virtual bool sendData() = 0;

  void iterateAlways() override {
    if (millis() - lastSendTime > refreshTime * 1000) {
      if (!Supla::Network::IsReady()) {
        lastSendTime += 10000;
      } else {
        if (sendData())
          lastSendTime = millis();
        else
          lastSendTime += 10000;
      }
    }
  }

 protected:
  int refreshTime = 180;
  uint32_t lastSendTime = 0;
  Supla::LocalAction* sensors[MAXSENSORS];
  int options[MAXSENSORS];
  Supla::Network* network = nullptr;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_WEATHERSENDER_H_
