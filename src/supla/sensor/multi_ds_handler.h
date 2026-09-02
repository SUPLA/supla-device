// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_H_
#define SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_H_

#include "multi_ds_handler_base.h"

#include <DallasTemperature.h>
#include <OneWire.h>

namespace Supla {
namespace Sensor {

class MultiDsHandler : public MultiDsHandlerBase {
 public:
  explicit MultiDsHandler(SuplaDeviceClass *sdc, uint8_t pin) :
      MultiDsHandlerBase(sdc, pin), oneWire(pin) {}

  ~MultiDsHandler() {}

  void onInit() override {
    dallasTemperature.setOneWire(&oneWire);
    dallasTemperature.begin();
    dallasTemperature.setWaitForConversion(false);

    MultiDsHandlerBase::onInit();
  }

  double getTemperature(const uint8_t *address) override {
    return dallasTemperature.getTempC(address);
  }

  double getTemperature(uint8_t idx) {
    if (idx >= MULTI_DS_MAX_DEVICES_COUNT) {
      return TEMPERATURE_NOT_AVAILABLE;
    }

    auto sensor = sensors[idx];
    if (sensor == nullptr) {
      return TEMPERATURE_NOT_AVAILABLE;
    }

    return sensor->getValue();
  }

  Supla::Sensor::MultiDsSensor * getThermometer(uint8_t idx) {
    if (idx < MULTI_DS_MAX_DEVICES_COUNT) {
      return sensors[idx];
    }

    return nullptr;
  }

  /**
   * Enables or disables synchronous (blocking) temperature conversion.
   *
   * By default, DallasTemperature operates in asynchronous mode (false),
   * meaning requestTemperatures() starts the conversion
   * and returns immediately (non-blocking mode). The caller is then
   * responsible for ensuring that enough time has passed before
   * reading the temperature value.
   *
   * When set to true, requestTemperatures() blocks execution until the DS18B20
   * conversion is complete (up to 750 ms at 12-bit resolution).
   *
   * This method is a wrapper for DallasTemperature::setWaitForConversion().
   */
  void setUseSynchronousCommunication(bool synchronous) {
    dallasTemperature.setWaitForConversion(synchronous);
  }

 protected:
  OneWire oneWire;
  DallasTemperature dallasTemperature;

  int refreshSensorsCount() override {
    oneWire.reset_search();
    dallasTemperature.begin();

    return dallasTemperature.getDeviceCount();
  }

  void requestTemperatures() override {
    dallasTemperature.requestTemperatures();
  }

  bool getSensorAddress(uint8_t *address, int index) override {
    return dallasTemperature.getAddress(address, index);
  }
};

};  // namespace Sensor
};  // namespace Supla


#endif  // SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_H_
