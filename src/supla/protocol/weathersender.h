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
    for (int i=0; i<MAXSENSORS; i++) {
      sensors[i] = nullptr;
    }
    lastSendTime = millis() - 100 * 1000;
  }

  void addSensor(Supla::SenorType type, Supla::Element* sensor, double shift = 0.0, double multiplier = 1.0, int option = 0) {
    SUPLA_LOG_DEBUG("aqi.eco: added sensor [%d]", type);
    sensors[type] = sensor;
    shifts[type] = shift;
    multipliers[type] = multiplier;
    options[type] = option;
  }

  double getSensorValue(int type) {
    double value = NAN;

    if (sensors[type] == nullptr) {
      return value;
    }

    switch(type) {
      case Supla::SenorType::PM1:
        value = ((Supla::Sensor::ParticleMeter*)(sensors[type]))->getPM1();
        break;
      case Supla::SenorType::PM2_5:
        value = ((Supla::Sensor::ParticleMeter*)(sensors[type]))->getPM2_5();
        break;
      case Supla::SenorType::PM4:
        value = ((Supla::Sensor::ParticleMeter*)(sensors[type]))->getPM4();
        break;
      case Supla::SenorType::PM10:
        value = ((Supla::Sensor::ParticleMeter*)(sensors[type]))->getPM10();
        break;
      case Supla::SenorType::TEMP:
        value = ((Supla::Sensor::ThermHygroMeter*)(sensors[type]))->getTemp();
        break;
      case Supla::SenorType::HUMI:
        value = ((Supla::Sensor::ThermHygroMeter*)(sensors[type]))->getHumi();
        break;
      case Supla::SenorType::PRESS:
        // TODO: Supla::Sensor:Pressure
        value = ((Supla::Sensor::ThermHygroPressMeter*)(sensors[type]))->getPressure()*100;
        break;
      case Supla::SenorType::LIGHT:
        value = ((Supla::Sensor::GeneralPurposeMeasurement*)(sensors[type]))->getValue();
        break;
      case Supla::SenorType::WIND:
        value = ((Supla::Sensor::Wind*)(sensors[type]))->getValue();
        break;
      case Supla::SenorType::RAIN:
        value = ((Supla::Sensor::Rain*)(sensors[type]))->getValue();
        break;
      case Supla::SenorType::CO2:
        value = ((Supla::Sensor::GeneralPurposeMeasurement*)(sensors[type]))->getValue();
        break;
    }

    value *= multipliers[type];
    value += shifts[type];

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
  Supla::Element* sensors[MAXSENSORS];
  double shifts[MAXSENSORS];
  double multipliers[MAXSENSORS];
  int options[MAXSENSORS];
  Supla::Network* network = nullptr;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_WEATHERSENDER_H_
