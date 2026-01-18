/*
   Copyright (C) malarz

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

// Dependencies:
// https://github.com/boeserfrosch/GuL_NovaFitness/tree/main

#ifndef SRC_SUPLA_SENSOR_PARTICLE_METER_SDS011_H_
#define SRC_SUPLA_SENSOR_PARTICLE_METER_SDS011_H_

#include <supla/sensor/particle_meter.h>
#include <SDS011.h>

namespace Supla {
namespace Sensor {

class ParticleMeterSDS011 : public Supla::Sensor::ParticleMeter {
 public:
  // rx_pin, tx_pin: pins to which the sensor is connected
  // refresh: time between readings (in minutes: 1-1440)
  explicit ParticleMeterSDS011(int rx_pin, int tx_pin, int refresh = 3)
      : ParticleMeter() {
    rxPin = rx_pin;
    txPin = tx_pin;
    if (refresh < 1) {
      refresh = 10;
    } else if (refresh > 1440) {
      refresh = 10;
    }
    refreshIntervalMs = refresh * 60 * 1000;

    // create GPM channel for PM2.5
    createPM2_5Channel();
    // create GPM channel for PM10
    createPM10Channel();
  }

  void onInit() override {
    // Setup and create instance of the PM1006K driver
    // The baud rate for the serial connection must be PM1006K::BAUD_RATE.
    Serial1.begin(9600, SERIAL_8N1, rxPin, txPin);
    sensor = new GuL::SDS011(Serial1);
    sensor->setToActiveReporting();
  }

  void iterateAlways() override {
    if (millis() - lastReadTime > refreshIntervalMs) {
      sensor->read();
      float valuePM2_5 = sensor->getPM2_5();
      float valuePM10 = sensor->getPM10();

      SUPLA_LOG_DEBUG("SDS011: PM2.5 read: %.0f", valuePM2_5);
      SUPLA_LOG_DEBUG("SDS011: PM10 read: %.0f", valuePM10);

      if (valuePM2_5 < 0 || valuePM10 < 0) {
        if (invalidCounter < 3) {
          invalidCounter++;
        } else {
          pm2_5value = NAN;
          pm10value = NAN;
        }
      } else {
        invalidCounter = 0;
        pm2_5value = valuePM2_5;
        pm10value = valuePM10;
      }

      lastReadTime = millis();
      pm2_5channel->setValue(pm2_5value);
      pm10channel->setValue(pm10value);
    }
  }

 protected:
  GuL::SDS011* sensor;
  int rxPin = 0;
  int txPin = 0;
  uint32_t refreshIntervalMs = 600000;
  uint32_t lastReadTime = 0;
  int invalidCounter = 0;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_PARTICLE_METER_SDS011_H_
