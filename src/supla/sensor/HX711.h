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

#ifndef SRC_SUPLA_SENSOR_HX711_H_
#define SRC_SUPLA_SENSOR_HX711_H_

/*
Dependency: https://github.com/olkal/HX711_ADC,
use library manager to install it
*/

#include <HX711_ADC.h>
#include <supla/log_wrapper.h>

#include "weight.h"
#include "../storage/storage.h"

namespace Supla {
namespace Sensor {

#pragma pack(push, 1)
struct HX711ConfigData {
  uint32_t tareOffset;
  float calValue;
};
#pragma pack(pop)

class HX711 : public Weight {
 public:
  HX711(int sdaPin, int sclPin, float defCalValue = 696.0)
      : hx711Sensor(sdaPin, sclPin), defCalValue(defCalValue) {
  }

  double getValue() {
    if ((hx711Sensor.getSPS() < 7) || (hx711Sensor.getSPS() > 100)) {
      value = WEIGHT_NOT_AVAILABLE;
      SUPLA_LOG_DEBUG(
          "HX711 measured sampling rate: %.2f HZ is abnormal, check connection"
           , hx711Sensor.getSPS());
    } else {
      value = (value < 0 && value > -1) ? 0 : value;
      // Displaying one decimal in Cloud
      int tmp = static_cast<int>(value*10);
      value = static_cast<double>(tmp/10.0);
    }
    return value;
  }

  void onInit() {
    hx711Sensor.begin();
    hx711Sensor.setTareOffset(tareOffset);
    uint64_t stabilizingTime = 2000;
    bool _tare = false;
    hx711Sensor.start(stabilizingTime, _tare);
    if (hx711Sensor.getTareTimeoutFlag()) {
      SUPLA_LOG_DEBUG("HX711 timeout, check connection");
    } else {
      calFactor = (calValue > 0) ? calValue : defCalValue;
      hx711Sensor.setCalFactor(calFactor);
      SUPLA_LOG_DEBUG("HX711 startup is complete");
    }
    uint32_t wait = millis();
    while (!hx711Sensor.update() && millis() - wait <= 100) {}
    SUPLA_LOG_INFO("HX711 calibration value: %.2f", hx711Sensor.getCalFactor());
    SUPLA_LOG_INFO(
        "HX711 measured conversion time ms: %.2f",
         hx711Sensor.getConversionTime());
    SUPLA_LOG_DEBUG(
        "HX711 measured sampling rate: %.2f HZ", hx711Sensor.getSPS());
    SUPLA_LOG_INFO(
        "HX711 measured settling time ms: %d", hx711Sensor.getSettlingTime());
    channel.setNewValue(getValue());
  }

  void calibrateScales(float knownMass = 0) {
    if (!readyToCalibration) {
      SUPLA_LOG_DEBUG("HX711 please do tare");
    } else {
      hx711Sensor.update();
      hx711Sensor.refreshDataSet();
      if (knownMass > 0) {
        calValue = hx711Sensor.getNewCalibration(knownMass);
        SUPLA_LOG_DEBUG("HX711 new calibration value: %.2f", calValue);
        Supla::Storage::ScheduleSave(100);
      } else {
        SUPLA_LOG_DEBUG("HX711 known mass must be greater than zero");
      }
    }
    readyToCalibration = false;
  }

  void tareScales() {
    hx711Sensor.tare();
    tareOffset = hx711Sensor.getTareOffset();
    SUPLA_LOG_DEBUG("HX711 new tare offset: %d", tareOffset);
    Supla::Storage::ScheduleSave(100);
    readyToCalibration = true;
  }

  void iterateAlways() {
    // According to the library, the sampling frequency
    // must be in the range 7 - 100 Hz. Now, it's around 10.5 Hz.
    if (hx711Sensor.update()) {
      value = hx711Sensor.getData();
    }
    if (millis() - lastReadTime > 500) {
      lastReadTime = millis();
      channel.setNewValue(getValue());
    }
  }

  void onLoadState() {
    HX711ConfigData data = HX711ConfigData();
    if (Supla::Storage::ReadState((unsigned char *)&data, sizeof(data))) {
      tareOffset = data.tareOffset;
      calValue = data.calValue;
    }
     SUPLA_LOG_DEBUG(
        "HX711 settings restored from storage. Tare offset: %d; "
        "calibration value: %.2f", tareOffset, calValue);
  }

  void onSaveState() {
    HX711ConfigData data;
    data.tareOffset = tareOffset;
    data.calValue = calValue;

    Supla::Storage::WriteState((unsigned char *)&data, sizeof(data));
  }

  float getTareOffset() {
    return tareOffset;
  }

  float getCalFactor() {
    return calFactor;
  }

 protected:
  ::HX711_ADC hx711Sensor;
  double value = 0;
  uint32_t tareOffset = 0;
  float calFactor = 0;
  float defCalValue;
  float calValue = 0;
  bool readyToCalibration = false;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_HX711_H_
