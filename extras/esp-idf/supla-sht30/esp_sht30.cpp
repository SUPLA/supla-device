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

#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <driver/i2c_master.h>
#include <supla/network/network.h>

#include "esp_sht30.h"

Supla::Sensor::SHT30::SHT30(Supla::I2CDriver *driver, uint8_t addr)
    : addr(addr), driver(driver) {
}

void Supla::Sensor::SHT30::onInit() {
  if (driver == nullptr) {
    return;
  }

  if (!driver->isInitialized()) {
    driver->initialize();
    if (!driver->isInitialized()) {
      return;
    }
  }

  const uint32_t frequency = 40000;

  driver->aquire();
  devHandle = driver->addDevice(addr, frequency);
  driver->release();

  if (devHandle == nullptr) {
    SUPLA_LOG_WARNING("SHT30[0x%2X]: Failed to add i2c device", addr);
  } else {
    SUPLA_LOG_DEBUG("SHT30[0x%2X]: I2C device added", addr);
  }

  channel.setNewValue(getTemp(), getHumi());
}

void Supla::Sensor::SHT30::readSensor() {
  if (driver == nullptr || !driver->isInitialized() || devHandle == nullptr) {
    return;
  }


  // TODO(klew): add CRC verification
  uint8_t payload[2] = {0x2C, 0x06};
  uint8_t buff[6] = {};

  driver->aquire();
  auto ret = i2c_master_transmit(*devHandle, payload, 2, 200);
  driver->release();
  if (ret != ESP_OK) {
    SUPLA_LOG_DEBUG("SHT30: failed to start measurement");
    return;
  }

  delay(30);
  driver->aquire();
  ret = i2c_master_receive(*devHandle, buff, 6, 400);
  driver->release();
  if (ret != ESP_OK) {
    SUPLA_LOG_DEBUG("SHT30: failed to read measurement");
  }
  Supla::Network::printData("SHT30", buff, 6);

  double temperature = 0;
  double humidity = 0;
  uint16_t tempRaw = buff[0] << 8 | buff[1];
  uint16_t humiRaw = buff[3] << 8 | buff[4];
  temperature = -45 + 1.75 * tempRaw / 655.35;
  humidity = humiRaw / 655.35;
  if (humidity < 0) {
    humidity = 0;
  }
  if (humidity > 100) {
    humidity = 100;
  }
  SUPLA_LOG_DEBUG("Temp: %.2f, humi %.2f", temperature, humidity);
  lastValidTemp = temperature;
  lastValidHumi = humidity;
}

double Supla::Sensor::SHT30::getTemp() {
  readSensor();
  return lastValidTemp;
}

double Supla::Sensor::SHT30::getHumi() {
  return lastValidHumi;
}

