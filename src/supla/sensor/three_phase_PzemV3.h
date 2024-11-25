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

#ifndef SRC_SUPLA_SENSOR_THREE_PHASE_PZEMV3_H_
#define SRC_SUPLA_SENSOR_THREE_PHASE_PZEMV3_H_

#include <Arduino.h>
// dependence: Arduino library for the Updated PZEM-004T v3.0 Power and Energy
// meter  https://github.com/mandulaj/PZEM-004T-v30
#include <PZEM004Tv30.h>
#if defined(PZEM004_SOFTSERIAL)
#include <SoftwareSerial.h>
#endif
#include <supla/time.h>

#include "electricity_meter.h"

namespace Supla {
namespace Sensor {

class ThreePhasePZEMv3 : public ElectricityMeter {
 public:
#if defined(PZEM004_SOFTSERIAL)
  ThreePhasePZEMv3(int8_t pinRX1,
                   int8_t pinTX1,
                   int8_t pinRX2,
                   int8_t pinTX2,
                   int8_t pinRX3,
                   int8_t pinTX3)
      : pzem{PZEM004Tv30(pinRX1, pinTX1),
             PZEM004Tv30(pinRX2, pinTX2),
             PZEM004Tv30(pinRX3, pinTX3)} {
  }
#endif

#if defined(ESP32)
  ThreePhasePZEMv3(HardwareSerial *serial1,
                   int8_t pinRx1,
                   int8_t pinTx1,
                   HardwareSerial *serial2,
                   int8_t pinRx2,
                   int8_t pinTx2,
                   HardwareSerial *serial3,
                   int8_t pinRx3,
                   int8_t pinTx3)
      : pzem{PZEM004Tv30(serial1, pinRx1, pinTx1),
             PZEM004Tv30(serial2, pinRx2, pinTx2),
             PZEM004Tv30(serial3, pinRx3, pinTx3)} {
  }
#else
  ThreePhasePZEMv3(HardwareSerial *serial1,
                   HardwareSerial *serial2,
                   HardwareSerial *serial3)
      : pzem{PZEM004Tv30(serial1), PZEM004Tv30(serial2), PZEM004Tv30(serial3)} {
  }
#endif

  void onInit() {
    lastReadTime = millis();
    readValuesFromDevice();
    updateChannelValues();
  }

  virtual void readValuesFromDevice() {
    bool atLeatOnePzemWasRead = false;
    for (int i = 0; i < 3; i++) {
      float current = pzem[i].current();
      // If current reading is NAN, we assume that PZEM there is no valid
      // communication with PZEM. Sensor shouldn't show any data
      if (isnan(current)) {
        resetReadParametersForPhase(i);
        continue;
      }

      atLeatOnePzemWasRead = true;

      float voltage = pzem[i].voltage();
      float active = pzem[i].power();
      float apparent = (voltage * current);
      float reactive = 0;
      if (apparent > active) {
        reactive = sqrt(apparent * apparent - active * active);
      } else {
        reactive = 0;
      }

      setVoltage(i, voltage * 100);
      setCurrent(i, current * 1000);
      setPowerActive(i, active * 100000);
      setFwdActEnergy(i, pzem[i].energy() * 100000);
      setPowerFactor(i, pzem[i].pf() * 1000);
      setPowerApparent(i, apparent * 100000);
      setPowerReactive(i, reactive * 10000);

      setFreq(pzem[i].frequency() * 100);
    }

    if (!atLeatOnePzemWasRead) {
      resetReadParameters();
    }
  }

  void resetStorage() {
    for (int i = 0; i < 3; i++) {
      pzem[i].resetEnergy();
    }
  }

 protected:
  PZEM004Tv30 pzem[3];
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THREE_PHASE_PZEMV3_H_
