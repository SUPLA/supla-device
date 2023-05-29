/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "esp_adc_driver.h"
#include <supla/log_wrapper.h>

using Supla::ADCDriver;

ADCDriver::ADCDriver(adc_unit_t unit) : unitId(unit) {
}

void ADCDriver::initialize() {
  if (!isInitialized()) {
    adc_oneshot_unit_init_cfg_t initConfig = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    auto err = adc_oneshot_new_unit(&initConfig, &adcHandle);

    if (err == ESP_OK) {
      SUPLA_LOG_INFO("ADC driver initialized, %d", unitId);
      initialized = true;
    } else {
      SUPLA_LOG_WARNING("Failed to init ADC (err: %d)", err);
    }
  }
}

void ADCDriver::initializeChannel(int gpio) {
  if (!isInitialized()) {
    initialize();
  }

  adc_oneshot_chan_cfg_t config = {
    .atten = attenuation,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
  };

  auto channelId = getChannelId(gpio);

  if (!errorFlag) {
    auto err = adc_oneshot_config_channel(adcHandle, channelId, &config);
    if (err != ESP_OK) {
      SUPLA_LOG_WARNING("Failed to configure ADC channel (err: %d)", err);
      errorFlag = true;
    }

    if (!calibrationDone) {
      esp_err_t ret = ESP_FAIL;
      bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
      if (!calibrated) {
        SUPLA_LOG_INFO("Trying to calibrate ADC: Curve Fitting");
        adc_cali_curve_fitting_config_t calibrationConfig = {};
        calibrationConfig.unit_id = unitId;
        calibrationConfig.atten = attenuation;
        calibrationConfig.bitwidth = ADC_BITWIDTH_DEFAULT;
        ret = adc_cali_create_scheme_curve_fitting(&calibrationConfig,
            &calibrationHandle);
        if (ret == ESP_OK) {
          calibrated = true;
        }
      }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
      if (!calibrated) {
        SUPLA_LOG_INFO("Trying to calibrate ADC: Line Fitting");
        adc_cali_line_fitting_config_t calibrationConfig = {};
        calibrationConfig.unit_id = unitId;
        calibrationConfig.atten = attenuation;
        calibrationConfig.bitwidth = ADC_BITWIDTH_DEFAULT;
        ret = adc_cali_create_scheme_line_fitting(&calibrationConfig,
            &calibrationHandle);
        if (ret == ESP_OK) {
          calibrated = true;
        }
      }
#endif

      if (ret == ESP_OK) {
        SUPLA_LOG_INFO("Calibration Success");
        calibrationDone = true;
      } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        SUPLA_LOG_WARNING("eFuse not burnt, skip software calibration");
      } else {
        SUPLA_LOG_WARNING("Failed to calibrate ADC (err: %d)", ret);
      }
    }
  }
}

adc_channel_t ADCDriver::getChannelId(int gpio) {
  adc_unit_t adcUnitCorrespondingToGpio = {};
  adc_channel_t channelId = {};
  auto err =
    adc_oneshot_io_to_channel(gpio, &adcUnitCorrespondingToGpio, &channelId);

  if (err != ESP_OK) {
    SUPLA_LOG_WARNING(
        "Failed to get channel id for GPIO %d (err: %d)", gpio, err);
    errorFlag = true;
    return ADC_CHANNEL_0;
  }

  if (adcUnitCorrespondingToGpio != unitId) {
    SUPLA_LOG_WARNING("GPIO %d is not configured for ADC unit %d",
        gpio,
        adcUnitCorrespondingToGpio);
    errorFlag = true;
    return ADC_CHANNEL_0;
  }

  return channelId;
}

bool ADCDriver::isInitialized() const {
  return initialized;
}

int ADCDriver::getMeasuement(int gpio) {
  if (!isInitialized() && errorFlag) {
    static bool showErrorLog = true;
    if (showErrorLog) {
      SUPLA_LOG_ERROR("ADC driver not initialized or in error state");
      showErrorLog = false;
    }
    return -1;
  }

  adc_channel_t channelId = getChannelId(gpio);
  int measurement = -1;
  auto err = adc_oneshot_read(adcHandle, channelId, &measurement);
  if (err != ESP_OK) {
    SUPLA_LOG_WARNING("Failed to read ADC measurement (err: %d)", err);
    return -1;
  }

  if (calibrationDone) {
    int voltage = 0;
    if (ESP_OK ==
        adc_cali_raw_to_voltage(calibrationHandle, measurement, &voltage)) {
      return voltage;
    }
  }

  return measurement;
}
