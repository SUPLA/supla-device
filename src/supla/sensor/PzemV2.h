// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_PZEMV2_H_
#define SRC_SUPLA_SENSOR_PZEMV2_H_

#include <Arduino.h>
// dependence: Arduino communication library for Peacefair PZEM-004T Energy
// monitor  https://github.com/olehs/PZEM004T
#include <PZEM004T.h>
#include <SoftwareSerial.h>
#include <supla/time.h>

#include "one_phase_electricity_meter.h"

namespace Supla {
namespace Sensor {

class PZEMv2 : public OnePhaseElectricityMeter {
 public:
  PZEMv2(int8_t pinRX, int8_t pinTX) : pzem(pinRX, pinTX), ip(192, 168, 1, 1) {
  }

  explicit PZEMv2(HardwareSerial *serial) : pzem(serial), ip(192, 168, 1, 1) {
  }

  void onInit() {
    lastReadTime = millis();
    pzem.setAddress(ip);
    readValuesFromDevice();
    updateChannelValues();
  }

  virtual void readValuesFromDevice() {
    float current = pzem.current(ip);
    // If current reading is NAN, we assume that PZEM there is no valid
    // communication with PZEM. Sensor shouldn't show any data
    if (current == PZEM_ERROR_VALUE) {
      resetReadParameters();
      return;
    }

    float powerFactor = 0;
    float reactive = 0;
    float voltage = pzem.voltage(ip);
    float active = pzem.power(ip);
    float apparent = (voltage * current);
    if (apparent > active) {
      reactive = sqrt(apparent * apparent - active * active);
    } else {
      reactive = 0;
    }
    if (active > apparent) {
      powerFactor = 1;
    } else if (apparent == 0) {
      powerFactor = 0;
    } else {
      powerFactor = (active / apparent);
    }

    setVoltage(0, voltage * 100);
    setCurrent(0, current * 1000);
    setPowerActive(0, active * 100000);
    setFwdActEnergy(0, pzem.energy(ip) * 100);
    setPowerApparent(0, apparent * 100000);
    setPowerReactive(0, reactive * 10000);
    setPowerFactor(0, powerFactor * 1000);
  }

  PZEM004T pzem;
  IPAddress ip;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_PZEMV2_H_
