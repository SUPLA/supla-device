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

#ifndef SRC_SUPLA_SENSOR_PZEMV3_H_
#define SRC_SUPLA_SENSOR_PZEMV3_H_

#include <Arduino.h>
// dependence: Arduino library for the Updated PZEM-004T v3.0 Power and Energy
// meter  https://github.com/mandulaj/PZEM-004T-v30
#include <PZEM004Tv30.h>

#define PZEM_0_DEFAULT_ADDR 0xF8

#if defined(PZEM004_SOFTSERIAL)
#include <SoftwareSerial.h>
#endif
#include <supla/time.h>

#include "one_phase_electricity_meter.h"

namespace Supla {
namespace Sensor {

class PZEMv3 : public OnePhaseElectricityMeter {
 public:
#if defined(PZEM004_SOFTSERIAL)
  PZEMv3(uint8_t pinRX1,
         uint8_t pinTX1,
         uint8_t pzem_addr = PZEM_0_DEFAULT_ADDR)
      : pzem{PZEM004Tv30(pinRX1, pinTX1, pzem_addr)} {
  }
#endif

#if defined(ESP32)
  PZEMv3(HardwareSerial *serial,
         uint8_t pinRx1,
         uint8_t pinTx1,
         uint8_t pzem_addr = PZEM_0_DEFAULT_ADDR)
      : pzem{PZEM004Tv30(serial, pinRx1, pinTx1, pzem_addr)} {
  }
#else
  explicit PZEMv3(HardwareSerial *serial,
                  uint8_t pzem_addr = PZEM_0_DEFAULT_ADDR)
      : pzem{PZEM004Tv30(serial, pzem_addr)} {
  }
#endif

  void onInit() {
    lastReadTime = millis();
    readValuesFromDevice();
    updateChannelValues();
  }

  virtual void readValuesFromDevice() {
    float current = pzem.current();
    // If current reading is NAN, we assume that PZEM there is no valid
    // communication with PZEM. Sensor shouldn't show any data
    if (isnan(current)) {
      resetReadParameters();
      return;
    }

    float voltage = pzem.voltage();
    float active = pzem.power();
    float apparent = (voltage * current);
    float reactive = 0;
    if (apparent > active) {
      reactive = sqrt(apparent * apparent - active * active);
    } else {
      reactive = 0;
    }

    setVoltage(0, voltage * 100);
    setCurrent(0, current * 1000);
    setPowerActive(0, active * 100000);
    setFwdActEnergy(0, pzem.energy() * 100000);
    setFreq(pzem.frequency() * 100);
    setPowerFactor(0, pzem.pf() * 1000);
    setPowerApparent(0, apparent * 100000);
    setPowerReactive(0, reactive * 100000);
  }

  void resetStorage() {
    pzem.resetEnergy();
  }

 protected:
  PZEM004Tv30 pzem;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_PZEMV3_H_
