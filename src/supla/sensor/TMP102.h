/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#pragma once

/*
Dependency: https://github.com/sparkfun/SparkFun_TMP102_Arduino_Library
Use library manager to install it
*/

#include <SparkFunTMP102.h>

#include <supla/log_wrapper.h>
#include "thermometer.h"

namespace Supla {
namespace Sensor {

class TMP102 : public Thermometer {
 public:
  struct Config {
    float hTemp = 70.0;
    float lTemp = 65.0;
    bool extMode = false;
    bool alertPolarity = false;
    uint8_t fault = 0;
    bool alertMode = false;
    double thresholdPercentage = 30.0;
  };

  explicit TMP102(uint8_t address,
                  Supla::Mutex *mutex,
                  TwoWire *wire,
                  const Config &cfg)
      : address_(address),
        mutex_(mutex),
        wire_(wire),
        cfg_(cfg) {
  }

  explicit TMP102(uint8_t address = 0x48,
                  Supla::Mutex *mutex = nullptr,
                  TwoWire *wire = &Wire)
      : TMP102(address, mutex, wire, Config{}) {
  }

  void onInit() override {
    initSensor(address_, wire_);
    channel.setNewValue(getTemp());
  }

  double getTemp() override {
    double t = TEMPERATURE_NOT_AVAILABLE;
    if (mutex_) mutex_->lock();
    if (isConnected_) {
      t = tmp102_.readTempC();
    }
    if (mutex_) mutex_->unlock();
    if (t == TEMPERATURE_NOT_AVAILABLE) {
      return t;
    }
    if (t < -40.0 || t > 125.0) {
      SUPLA_LOG_WARNING("[TMP102] invalid reading: %.2f", t);
      return TEMPERATURE_NOT_AVAILABLE;
    }
    if (lastValidTemperature_ != TEMPERATURE_NOT_AVAILABLE) {
      double diff = percentageDifference(t, lastValidTemperature_);
      if (diff > cfg_.thresholdPercentage) {
        SUPLA_LOG_DEBUG("[TMP102] rejected value: %.2f (diff: %d%%)", t,
                                                        static_cast<int>(diff));
        return lastValidTemperature_;
      }
    }
    lastValidTemperature_ = std::round(t * 100) / 100.0;
    return lastValidTemperature_;
  }

  bool getAlertState() {
    if (mutex_) mutex_->lock();
    bool raw = tmp102_.alert();
    if (mutex_) mutex_->unlock();
    return cfg_.alertMode ? raw : !raw;
  }

 protected:
  ::TMP102 tmp102_;
  Config cfg_;
  TwoWire *wire_ = nullptr;
  Supla::Mutex *mutex_ = nullptr;
  uint8_t address_ = 0x48;
  bool isConnected_ = false;
  double lastValidTemperature_ = TEMPERATURE_NOT_AVAILABLE;

  void initSensor(uint8_t address, TwoWire *wire) {
    if (mutex_) mutex_->lock();
    if (tmp102_.begin(address, *wire)) {
      SUPLA_LOG_DEBUG("TMP102 connected at 0x%x", address);
      tmp102_.wakeup();
      tmp102_.setHighTempC(cfg_.hTemp);
      tmp102_.setLowTempC(cfg_.lTemp);
      tmp102_.setExtendedMode(cfg_.extMode);
      tmp102_.setAlertPolarity(cfg_.alertPolarity);
      tmp102_.setFault(cfg_.fault);
      tmp102_.setAlertMode(cfg_.alertMode);
      isConnected_ = true;
    } else {
      SUPLA_LOG_ERROR("Unable to find TMP102 at 0x%x", address);
    }
    if (mutex_) mutex_->unlock();
  }

  static double percentageDifference(double a, double b) {
    return std::abs(a - b) / b * 100.0;
  }
};

};  // namespace Sensor
};  // namespace Supla
