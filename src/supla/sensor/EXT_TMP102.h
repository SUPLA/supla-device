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
#include "thermometer.h"
#include <supla/log_wrapper.h>

namespace Supla {
namespace Sensor {

class ExtTMP102 : public Thermometer {
 public:
  explicit ExtTMP102(uint8_t address = 0x48,
                     TwoWire *wire = &Wire,
                     float hTemp = 70.0,
                     float lTemp = 65.0,
                     bool extMode = false,
                     bool alertPolarity = false,
                     uint8_t fault = 0,
                     bool alertMode = false) : alertMode_(alertMode) {
    if (!tmp102_.begin(address, *wire)) {
      SUPLA_LOG_DEBUG("Unable to find TMP102 at address: 0x%x", address);
    } else {
      SUPLA_LOG_DEBUG("TMP102 is connected at address: 0x%x", address);
      tmp102_.wakeup();
      tmp102_.setHighTempC(hTemp);
      tmp102_.setLowTempC(lTemp);
      tmp102_.setExtendedMode(extMode);
      tmp102_.setAlertPolarity(alertPolarity);
      tmp102_.setFault(fault);
      tmp102_.setAlertMode(alertMode);
      isConnected_ = true;
    }
  }

  void onInit() override {
    channel.setNewValue(getTemp());
  }

  double getTemp() override {
    if (isConnected_) {
      double temp = tmp102_.readTempC();
      if (temp >= -55.0 && temp <= 128.0) {
        return round(temp * 100) / 100.0;
      } else {
        SUPLA_LOG_DEBUG("[TMP102] invalid temperature reading: %f", temp);
        return TEMPERATURE_NOT_AVAILABLE;
      }
    } else {
      return TEMPERATURE_NOT_AVAILABLE;
    }
  }

  bool getAlertState() {
    bool raw = tmp102_.alert();
    return alertMode_ ? raw : !raw;
  }

 protected:
  ::TMP102 tmp102_;
  bool alertMode_;
  bool isConnected_ = false;
};

}; // namespace Sensor
}; // namespace Supla