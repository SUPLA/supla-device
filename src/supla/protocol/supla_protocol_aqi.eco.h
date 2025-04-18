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

// It use ca. 20kB RAM, so ESP32 is highly recomended

#ifndef SRC_SUPLA_PROTOCOL_AQUECO_H_
#define SRC_SUPLA_PROTOCOL_AQUECO_H_

#include <supla/sensor/general_purpose_measurement.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/therm_hygro_press_meter.h>
#include <supla/sensor/rain.h>
#include <supla/sensor/wind.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "supla_sensor_particle_meter.h"
#include "supla_protocol_websender.h"

namespace Supla {
namespace Protocol {
class AQIECO : public Supla::Protocol::WebSender {
 public:
  explicit AQIECO(Supla::Network* _network, char token[], int refresh=180, char server[]="api.aqi.eco", int id=0);

  bool sendData() override;
  
 private:
  char apiToken[33];
  char serverAddress[33];
  uint32_t sensorId = 0;
};
}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_AQUECO_H_