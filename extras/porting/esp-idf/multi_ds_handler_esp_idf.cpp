// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "multi_ds_handler_esp_idf.h"

#include <ds18b20.h>
#include <owb.h>
#include <owb_gpio.h>

#include <string.h>

#include <supla/log_wrapper.h>

namespace Supla {
namespace Sensor {

namespace {

constexpr double kDisconnectedTemperature = -127.0;
constexpr uint8_t kDs18b20FamilyCode = 0x28;

}  // namespace

class MultiDsHandlerEspIdf::Impl {
 public:
  explicit Impl(uint8_t pin) : pin(pin) {}

  ~Impl() {
    clearDevices();
    if (bus != nullptr) {
      owb_uninitialize(bus);
      bus = nullptr;
    }
  }

  bool initialize() {
    if (bus != nullptr) {
      return true;
    }

    SUPLA_LOG_DEBUG("MultiDS ESP-IDF: Initializing OneWire bus on GPIO %u",
                    pin);
    bus = owb_gpio_initialize(&driverInfo, pin);
    if (bus == nullptr) {
      SUPLA_LOG_ERROR("MultiDS ESP-IDF: OneWire initialization failed");
      return false;
    }

    owb_use_crc(bus, true);
    return true;
  }

  int scan() {
    if (!initialize()) {
      return 0;
    }

    clearDevices();

    OneWireBus_SearchState searchState = {};
    bool found = false;
    owb_search_first(bus, &searchState, &found);
    while (found) {
      if (searchState.rom_code.bytes[0] == kDs18b20FamilyCode &&
          deviceCount < MULTI_DS_MAX_DEVICES_COUNT) {
        memcpy(addresses[deviceCount], searchState.rom_code.bytes, 8);
        devices[deviceCount] = ds18b20_malloc();
        if (devices[deviceCount] == nullptr) {
          SUPLA_LOG_ERROR("MultiDS ESP-IDF: DS18B20 allocation failed");
          break;
        }

        ds18b20_init(devices[deviceCount], bus, searchState.rom_code);
        ds18b20_use_crc(devices[deviceCount], true);
        ds18b20_set_resolution(devices[deviceCount],
                               DS18B20_RESOLUTION_12_BIT);

        deviceCount++;
      }

      owb_search_next(bus, &searchState, &found);
    }

    if (deviceCount > 0) {
      bool parasiticPower = false;
      if (ds18b20_check_for_parasite_power(bus, &parasiticPower) ==
          DS18B20_OK) {
        owb_use_parasitic_power(bus, parasiticPower);
      }
    }

    if (deviceCount != lastReportedDeviceCount) {
      SUPLA_LOG_DEBUG("MultiDS ESP-IDF: Found %d DS18B20 device(s)",
                      deviceCount);
      lastReportedDeviceCount = deviceCount;
    }
    return deviceCount;
  }

  void requestTemperatures() {
    if (!initialize()) {
      return;
    }
    if (deviceCount == 0) {
      scan();
    }
    if (deviceCount == 0) {
      return;
    }

    ds18b20_convert_all(bus);
  }

  bool getAddress(uint8_t *address, int index) const {
    if (address == nullptr || index < 0 || index >= deviceCount) {
      return false;
    }
    memcpy(address, addresses[index], 8);
    return true;
  }

  double getTemperature(const uint8_t *address) {
    if (address == nullptr) {
      return kDisconnectedTemperature;
    }

    int index = -1;
    for (int i = 0; i < deviceCount; i++) {
      if (memcmp(address, addresses[i], 8) == 0) {
        index = i;
        break;
      }
    }
    if (index < 0) {
      return kDisconnectedTemperature;
    }

    // MultiDsHandlerBase waits for the conversion to complete before reading.
    float reading = 0;
    if (ds18b20_read_temp(devices[index], &reading) != DS18B20_OK ||
        reading == 85.0f) {
      return kDisconnectedTemperature;
    }
    return reading;
  }

 private:
  void clearDevices() {
    for (int i = 0; i < MULTI_DS_MAX_DEVICES_COUNT; i++) {
      if (devices[i] != nullptr) {
        ds18b20_free(&devices[i]);
      }
      memset(addresses[i], 0, sizeof(addresses[i]));
    }
    deviceCount = 0;
  }

  uint8_t pin = 0;
  ::OneWireBus *bus = nullptr;
  owb_gpio_driver_info driverInfo = {};
  DS18B20_Info *devices[MULTI_DS_MAX_DEVICES_COUNT] = {};
  uint8_t addresses[MULTI_DS_MAX_DEVICES_COUNT][8] = {};
  int deviceCount = 0;
  int lastReportedDeviceCount = -1;
};

MultiDsHandlerEspIdf::MultiDsHandlerEspIdf(SuplaDeviceClass *sdc, uint8_t pin)
    : MultiDsHandlerBase(sdc, pin), impl(new Impl(pin)) {
}

MultiDsHandlerEspIdf::~MultiDsHandlerEspIdf() {
  delete impl;
  impl = nullptr;
}

void MultiDsHandlerEspIdf::onInit() {
  if (impl != nullptr) {
    impl->initialize();
    impl->scan();
  }
  MultiDsHandlerBase::onInit();
}

int MultiDsHandlerEspIdf::refreshSensorsCount() {
  return impl == nullptr ? 0 : impl->scan();
}

void MultiDsHandlerEspIdf::requestTemperatures() {
  if (impl != nullptr) {
    impl->requestTemperatures();
  }
}

bool MultiDsHandlerEspIdf::getSensorAddress(uint8_t *address, int index) {
  return impl != nullptr && impl->getAddress(address, index);
}

double MultiDsHandlerEspIdf::getTemperature(const uint8_t *address) {
  return impl == nullptr ? kDisconnectedTemperature
                         : impl->getTemperature(address);
}

}  // namespace Sensor
}  // namespace Supla
