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

#ifndef SRC_SUPLA_SENSOR_THREE_PHASE_PZEMV3_ADDR_H_
#define SRC_SUPLA_SENSOR_THREE_PHASE_PZEMV3_ADDR_H_

#include <Arduino.h>
// dependence: Arduino library for the Updated PZEM-004T v3.0 Power and Energy
// meter  https://github.com/mandulaj/PZEM-004T-v30
#include <PZEM004Tv30.h>
#if defined(PZEM004_SOFTSERIAL)
#include <SoftwareSerial.h>
#endif
#include <supla/time.h>

#include "electricity_meter.h"

#define PZEM_1_DEFAULT_ADDR 0x01
#define PZEM_2_DEFAULT_ADDR 0x02
#define PZEM_3_DEFAULT_ADDR 0x03

namespace Supla {
namespace Sensor {

class ThreePhasePZEMv3_ADDR : public ElectricityMeter {
 public:
#if defined(PZEM004_SOFTSERIAL)
  ThreePhasePZEMv3_ADDR(uint8_t pinRX1,
                        uint8_t pinTX1,
                        uint8_t pzem_1_addr = PZEM_1_DEFAULT_ADDR,
                        uint8_t pzem_2_addr = PZEM_2_DEFAULT_ADDR,
                        uint8_t pzem_3_addr = PZEM_3_DEFAULT_ADDR)
      : pzem{PZEM004Tv30(pinRX1, pinTX1, pzem_1_addr),
             PZEM004Tv30(pinRX1, pinTX1, pzem_2_addr),
             PZEM004Tv30(pinRX1, pinTX1, pzem_3_addr)} {
  }
#endif

#if defined(ESP32)
  ThreePhasePZEMv3_ADDR(HardwareSerial *serial,
                        uint8_t pinRx1,
                        uint8_t pinTx1,
                        uint8_t pzem_1_addr = PZEM_1_DEFAULT_ADDR,
                        uint8_t pzem_2_addr = PZEM_2_DEFAULT_ADDR,
                        uint8_t pzem_3_addr = PZEM_3_DEFAULT_ADDR)
      : pzem{PZEM004Tv30(serial, pinRx1, pinTx1, pzem_1_addr),
             PZEM004Tv30(serial, pinRx1, pinTx1, pzem_2_addr),
             PZEM004Tv30(serial, pinRx1, pinTx1, pzem_3_addr)} {
  }
#else
  ThreePhasePZEMv3_ADDR(HardwareSerial *serial,
                        uint8_t pzem_1_addr = PZEM_1_DEFAULT_ADDR,
                        uint8_t pzem_2_addr = PZEM_2_DEFAULT_ADDR,
                        uint8_t pzem_3_addr = PZEM_3_DEFAULT_ADDR)
      : pzem{PZEM004Tv30(serial, pzem_1_addr),
             PZEM004Tv30(serial, pzem_2_addr),
             PZEM004Tv30(serial, pzem_3_addr)} {
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
      setPowerReactive(i, reactive * 100000);

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

#endif  // SRC_SUPLA_SENSOR_THREE_PHASE_PZEMV3_ADDR_H_
