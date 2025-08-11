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

#ifndef SRC_SUPLA_SENSOR_SGP41_H_
#define SRC_SUPLA_SENSOR_SGP41_H_

// Dependency: Sensirion I2C SGP41 Arduino Library
// - use library manager to install it
// https://github.com/Sensirion/arduino-i2c-sgp41

#include "SensirionI2CSgp41.h"

#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/general_purpose_measurement.h>

namespace Supla {
namespace Sensor {
class SGP41 : public Element {
 public:
  explicit SGP41(ThermHygroMeter *ptr = nullptr) {
    th = ptr;
    vocchannel = new GeneralPurposeMeasurement();
    vocchannel->setDefaultUnitAfterValue("");
    vocchannel->setInitialCaption("VOC index");
    vocchannel->getChannel()->setDefaultIcon(8);
    vocchannel->setDefaultValuePrecision(1);

    noxchannel = new GeneralPurposeMeasurement();
    noxchannel->setDefaultUnitAfterValue("");
    noxchannel->setInitialCaption("NOx index");
    noxchannel->getChannel()->setDefaultIcon(8);
    noxchannel->setDefaultValuePrecision(1);
  }

  void onInit() override {
    sgp.begin(Wire);
  }

  GeneralPurposeMeasurement* getVOCchannel() {
    return vocchannel;
  }

  GeneralPurposeMeasurement* getNOXchannel() {
    return noxchannel;
  }

  void iterateAlways() override {
    // every 1 sec read from device
    if (millis() - lastReadTime > 1000) {
      readValuesFromDevice();
      lastReadTime = millis();
    }
  }

 private:
  void readValuesFromDevice() {
    uint16_t error;
    float temperature = TEMPERATURE_NOT_AVAILABLE;
    float humidity = HUMIDITY_NOT_AVAILABLE;
    if (th != nullptr) {
      temperature = th->getChannel()->getLastTemperature();
      humidity = th->getChannel()->getValueDoubleSecond();
    }
    uint16_t srawVoc = 0;
    uint16_t srawNox = 0;

    if (temperature != TEMPERATURE_NOT_AVAILABLE) {
      compensationT = static_cast<uint16_t>((temperature + 45) * 65535 / 175);
    } else {
      compensationT = defaultCompenstaionT;
    }

    if (humidity != HUMIDITY_NOT_AVAILABLE) {
      compensationRh = static_cast<uint16_t>(humidity * 65535 / 100);
    } else {
      compensationRh = defaultCompenstaionRh;
    }

    if (skipFirstReadingsCounter > 0) {
      error = sgp.executeConditioning(compensationRh, compensationT, srawVoc);
      skipFirstReadingsCounter--;
    } else {
      error = sgp.measureRawSignals(compensationRh, compensationT, srawVoc,
        srawNox);
    }

    if (error) {
      retryCount++;
      if (retryCount > 10) {
        vocchannel->setValue(NAN);
        noxchannel->setValue(NAN);
      }
    } else {
      retryCount = 0;
      vocchannel->setValue(srawVoc);
      noxchannel->setValue(srawNox);
    }
  }

 protected:
  uint16_t defaultCompenstaionRh = 0x8000;  // in ticks as defined by SGP41
  uint16_t defaultCompenstaionT = 0x6666;   // in ticks as defined by SGP41
  uint16_t compensationRh = 0;              // in ticks as defined by SGP41
  uint16_t compensationT = 0;               // in ticks as defined by SGP41
  int8_t retryCount = 0;
  ::SensirionI2CSgp41 sgp;
  GeneralPurposeMeasurement *vocchannel = nullptr;
  GeneralPurposeMeasurement *noxchannel = nullptr;
  ThermHygroMeter *th = nullptr;
  uint32_t lastReadTime = 0;
  int skipFirstReadingsCounter = 10;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SGP41_H_
