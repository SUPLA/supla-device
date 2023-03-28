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

#include "esp_sht30.h"

Supla::Sensor::SHT30::SHT30(int sda, int scl, uint8_t addr)
    : sda(sda), scl(scl), addr(addr) {
}
void Supla::Sensor::SHT30::onInit() {
  i2c_config_t conf = {};

  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = sda;
  conf.scl_io_num = scl;
  conf.sda_pullup_en = false;
  conf.scl_pullup_en = false;
  conf.master.clk_speed = 40000;

  i2c_param_config(I2C_NUM_0, &conf);

  esp_err_t err = (i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));

  if (err != ESP_OK) {
    SUPLA_LOG_WARNING("Failed to init i2c (%d)", err);
  }

  channel.setNewValue(getTemp(), getHumi());
}

void Supla::Sensor::SHT30::readSensor() {
  // TODO(klew): add CRC verification
  uint8_t registerToReadByte1 = 0x2C;
  uint8_t registerToReadByte2 = 0x06;
  uint8_t buff[6] = {};

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, addr << 1 | I2C_MASTER_WRITE, 1);
  i2c_master_write_byte(cmd, registerToReadByte1, 1);
  i2c_master_write_byte(cmd, registerToReadByte2, 1);
  i2c_master_stop(cmd);
  esp_err_t ret =
      i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  if (ret != ESP_OK) {
    SUPLA_LOG_DEBUG("SHT30: failed to send read request %d", ret);
  }

  delay(30);
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, addr << 1 | I2C_MASTER_READ, 1);
  i2c_master_read_byte(cmd, &buff[0], I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, &buff[1], I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, &buff[2], I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, &buff[3], I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, &buff[4], I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, &buff[5], I2C_MASTER_NACK);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  if (ret != ESP_OK) {
    SUPLA_LOG_DEBUG("SHT30: failed to read measurement");
  }
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
  SUPLA_LOG_DEBUG("Temp: %f, humi %f", temperature, humidity);
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

void Supla::Sensor::SHT30::iterateAlways() {
  if (millis() - lastReadTime > 2000) {
    lastReadTime = millis();
    channel.setNewValue(getTemp(), getHumi());
  }
}
