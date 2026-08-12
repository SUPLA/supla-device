// SPDX-FileCopyrightText: malarz
// SPDX-License-Identifier: GPL-2.0-or-later

// Dependencies:
// https://github.com/kevinlutzer/Arduino-PM1006K

#ifndef SRC_SUPLA_SENSOR_PARTICLE_METER_PM1006K_H_
#define SRC_SUPLA_SENSOR_PARTICLE_METER_PM1006K_H_

#include <supla/sensor/particle_meter.h>

#include <PM1006K.h>

namespace Supla {
namespace Sensor {
class ParticleMeterPM1006K : public Supla::Sensor::ParticleMeter {
 public:
  /*!
   *  @brief class constructor for PM1006K sensor
   *  @param rx_pin, tx_pin : pins to which the sensor is connected
   *  @param refresh : time between readings (in sec: 60-86400)
   *  @param fan : fan working time (in sec: 15-120)
   */
  explicit ParticleMeterPM1006K(int rx_pin, int tx_pin, int fan_pin = -1,
      int refresh = 600, int fan = 15)
      : ParticleMeter() {
    rxPin = rx_pin;
    txPin = tx_pin;
    if (refresh < 60) {
      refresh = 600;
    } else if (refresh > 86400) {
      refresh = 600;
    }
    if (fan < 15) {
      fan = 15;
    } else if (fan > 120) {
      fan = 120;
    } else if (fan > refresh) {
      fan = refresh;
    }
    refreshIntervalMs = refresh * 1000;
    fanTime = fan * 1000;

    // FAN setup
    fanPin = fan_pin;
    if (fanPin >= 0) {
      pinMode(fanPin, OUTPUT);
      fanOff = false;
      digitalWrite(fanPin, HIGH);
      SUPLA_LOG_DEBUG("PM1006K FAN: started & on");
    }

    // create GPM channel for PM2.5
    createPM2_5Channel();
  }

  void onInit() override {
    // Setup and create instance of the PM1006K driver
    // The baud rate for the serial connection must be PM1006K::BAUD_RATE.
    Serial1.begin(PM1006K::BAUD_RATE, SERIAL_8N1, rxPin, txPin);
    sensor = new PM1006K(&Serial1);
  }

  void iterateAlways() override {
    // enable fan fanTime before reading sensor
    if (millis() - lastReadTime > refreshIntervalMs-fanTime) {
      if ((fanPin >= 0)  && fanOff) {
        fanOff = false;
        digitalWrite(fanPin, HIGH);
        SUPLA_LOG_DEBUG("PM1006K FAN: on");
      }
    }

    if (millis() - lastReadTime > refreshIntervalMs) {
      double valuePM1 = NAN;
      double valuePM2_5 = NAN;
      double valuePM10 = NAN;
      if (!sensor->takeMeasurement()) {
        SUPLA_LOG_DEBUG("PM1006K: failed to take measurement");
      } else {
        valuePM1 = sensor->getPM1_0();
        SUPLA_LOG_DEBUG("PM1006K: PM1 read: %.0f", valuePM1);
        valuePM2_5 = sensor->getPM2_5();
        SUPLA_LOG_DEBUG("PM1006K: PM2.5 read: %.0f", valuePM2_5);
        valuePM10 = sensor->getPM10();
        SUPLA_LOG_DEBUG("PM1006K: PM10 read: %.0f", valuePM10);
      }

      if (isnan(valuePM2_5) || valuePM2_5 <= 0) {
        if (invalidCounter < 3) {
          invalidCounter++;
        } else {
          pm1value = NAN;
          pm2_5value = NAN;
          pm10value = NAN;
        }
      } else {
        invalidCounter = 0;
        pm1value = valuePM1;
        pm2_5value = valuePM2_5;
        pm10value = valuePM10;

        // switch faan off after successfull reading
        if ((fanPin >= 0) && (refreshIntervalMs !=fanTime)) {
          fanOff = true;
          digitalWrite(fanPin, LOW);
          SUPLA_LOG_DEBUG("PM1006K FAN: off");
        }
      }
      if (pm1channel != nullptr) {
        pm1channel->setValue(pm1value);
      }
      pm2_5channel->setValue(pm2_5value);
      if (pm10channel != nullptr) {
        pm10channel->setValue(pm10value);
      }
      lastReadTime = millis();
    }
  }

 protected:
  ::PM1006K* sensor = nullptr;
  int fanPin = -1;
  int rxPin = 0;
  int txPin = 0;
  bool fanOff = true;
  uint32_t fanTime = 15;
  int invalidCounter = 0;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_PARTICLE_METER_PM1006K_H_
