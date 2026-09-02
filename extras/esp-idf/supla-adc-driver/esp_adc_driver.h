// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_ADC_DRIVER_ESP_ADC_DRIVER_H_
#define EXTRAS_ESP_IDF_SUPLA_ADC_DRIVER_ESP_ADC_DRIVER_H_

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

namespace Supla {
class ADCDriver {
 public:
  explicit ADCDriver(adc_unit_t);
  ~ADCDriver();

  void initialize();
  void initializeChannel(int gpio);
  bool isInitialized() const;

  // Returns Voltage in mV (when ADC was properly calibrated, otherwise
  // raw reading from ADC is returned).
  // Returns -1 in case of error.
  int getMeasuement(int gpio);

 protected:
  adc_channel_t getChannelId(int gpio);

  adc_unit_t unitId;
  adc_cali_handle_t calibrationHandle;
  adc_atten_t attenuation = ADC_ATTEN_DB_12;
  bool initialized = false;
  bool errorFlag = false;
  bool calibrationDone = false;
  adc_oneshot_unit_handle_t adcHandle;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_ADC_DRIVER_ESP_ADC_DRIVER_H_
