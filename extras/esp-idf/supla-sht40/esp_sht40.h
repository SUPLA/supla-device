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

#ifndef EXTRAS_ESP_IDF_SUPLA_SHT40_ESP_SHT40_H_
#define EXTRAS_ESP_IDF_SUPLA_SHT40_ESP_SHT40_H_

#include <driver/i2c.h>

#include <supla/sensor/therm_hygro_meter.h>

namespace Supla {
namespace Sensor {
class SHT40 : public ThermHygroMeter {
 public:
  SHT40(int sda, int scl, uint8_t addr);

  void onInit() override;
  void iterateAlways() override;

  double getTemp() override;
  double getHumi() override;

  void readSensor();

 protected:
  double lastValidTemp = TEMPERATURE_NOT_AVAILABLE;
  double lastValidHumi = HUMIDITY_NOT_AVAILABLE;
  int8_t retryCountTemp = 0;
  int8_t retryCountHumi = 0;
  int sda = 0;
  int scl = 0;
  uint8_t addr = 0x44;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_SHT40_ESP_SHT40_H_
