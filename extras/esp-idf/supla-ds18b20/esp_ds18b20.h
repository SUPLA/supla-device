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

#ifndef EXTRAS_ESP_IDF_SUPLA_DS18B20_ESP_DS18B20_H_
#define EXTRAS_ESP_IDF_SUPLA_DS18B20_ESP_DS18B20_H_

#include <stdint.h>

#include <owb.h>
#include <owb_gpio.h>
#include <ds18b20.h>

#define MAX_DEVICES (16)
#define DS18B20_RESOLUTION (DS18B20_RESOLUTION_12_BIT)

#include <supla/log_wrapper.h>
#include <supla/sensor/thermometer.h>

#define SUPLA_DS_ADDRESS_SIZE 8

namespace Supla {
namespace Sensor {

class DS18B20;

class OneWireBus {
 public:
  explicit OneWireBus(uint8_t gpioNumber);
  void init();
  void addDsToList(Supla::Sensor::DS18B20 *);
  bool assignAddress(uint8_t *address);
  bool removeDs(uint8_t *address);

  void scanBus();
  void requestTemperatures();
  bool isMeasurementReady();

  double getTemp(const uint8_t *deviceAddress);
  bool isAlreadyRead(const uint8_t *deviceAddress);
  int getIndex(const uint8_t *deviceAddress);

  uint8_t gpio = 0;
  OneWireBus *nextBus = nullptr;
  uint32_t lastReadTime = 0;
  uint32_t lastMeasurementRequestTimeMs = 0;
  uint32_t conversionTimeMs = 750;

 protected:
  ::OneWireBus *owb = {};
  Supla::Sensor::DS18B20 *firstDs = nullptr;
  owb_gpio_driver_info driverInfo = {};
  DS18B20_Info *devices[MAX_DEVICES] = {};
  bool deviceTempRead[MAX_DEVICES] = {};
};

class DS18B20 : public Thermometer {
 public:
  static void clearAssignedAddress(uint8_t *deviceAddress);

  explicit DS18B20(uint8_t gpio, uint8_t *deviceAddress = nullptr);
  void iterateAlways() override;
  void onLoadConfig(SuplaDeviceClass *) override;
  void onInit() override;
  double getValue() override;
  void assignAddressIfNeeded();
  void releaseAddress();

  friend OneWireBus;

 protected:
  static Supla::Sensor::OneWireBus *firstOneWireBus;

  Supla::Sensor::DS18B20 *nextDs = nullptr;
  uint8_t address[SUPLA_DS_ADDRESS_SIZE] = {};
  Supla::Sensor::OneWireBus *myBus = {};
  int8_t retryCounter = 0;
  double lastValidValue = TEMPERATURE_NOT_AVAILABLE;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_DS18B20_ESP_DS18B20_H_
