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

#include "esp_ds18b20.h"

#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/time.h>

#include "supla/sensor/one_phase_electricity_meter.h"
#include "supla/sensor/thermometer.h"

void Supla::Sensor::DS18B20::onLoadConfig(SuplaDeviceClass *sdc) {
  Supla::Sensor::Thermometer::onLoadConfig(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "address");
    if (!cfg->getBlob(
            key, reinterpret_cast<char *>(address), SUPLA_DS_ADDRESS_SIZE)) {
      SUPLA_LOG_DEBUG("Failed to read DS address");
    }
    if (address[0] == 0) {
      getChannel()->setDefault(0);
    }
  }
  myBus->lastReadTime = 0;
}

void Supla::Sensor::DS18B20::onInit() {
  myBus->init();
  assignAddressIfNeeded();
  channel.setNewValue(TEMPERATURE_NOT_AVAILABLE);
}

Supla::Sensor::OneWireBus::OneWireBus(uint8_t gpioNumber) : gpio(gpioNumber) {
}

void Supla::Sensor::OneWireBus::init() {
  if (owb) {
    // bus is already initialized
    return;
  }

  SUPLA_LOG_DEBUG("Initializing OneWire bus at GPIO %d", gpio);

  owb = owb_gpio_initialize(&driverInfo, gpio);
  owb_use_crc(owb, true);

  scanBus();
}

void Supla::Sensor::DS18B20::assignAddressIfNeeded() {
  if (address[0] == 0) {
    if (myBus->assignAddress(address)) {
      auto cfg = Supla::Storage::ConfigInstance();
      if (cfg) {
        char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
        generateKey(key, "address");
        if (!cfg->setBlob(key,
                          reinterpret_cast<char *>(address),
                          SUPLA_DS_ADDRESS_SIZE)) {
          SUPLA_LOG_WARNING("Failed to write DS address to config");
        }
        cfg->saveWithDelay(2000);
        getChannel()->setDefault(SUPLA_CHANNELFNC_THERMOMETER);
      }
    }
  }
}

bool Supla::Sensor::OneWireBus::assignAddress(uint8_t *address) {
  // check if there is DS detected which is not already assigned to any channel
  for (int i = 0; i < MAX_DEVICES && devices[i]; i++) {
    auto dsPtr = firstDs;
    bool found = false;
    while (dsPtr && !found) {
      for (int j = 0; j < SUPLA_DS_ADDRESS_SIZE; j++) {
        if (dsPtr->address[j] !=
            devices[i]->rom_code.bytes[SUPLA_DS_ADDRESS_SIZE - 1 - j]) {
          break;
        }
        if (j == SUPLA_DS_ADDRESS_SIZE - 1) {
          found = true;
        }
      }

      dsPtr = dsPtr->nextDs;
    }
    if (!found) {
      char romCodeStr[17];
      owb_string_from_rom_code(
          devices[i]->rom_code, romCodeStr, sizeof(romCodeStr));
      SUPLA_LOG_DEBUG("DS assigned to channel: %s", romCodeStr);
      for (int j = 0; j < SUPLA_DS_ADDRESS_SIZE; j++) {
        address[j] = devices[i]->rom_code.bytes[SUPLA_DS_ADDRESS_SIZE - 1 - j];
      }
      return true;
    }
  }
  return false;
}

void Supla::Sensor::OneWireBus::requestTemperatures() {
  if (owb) {
    scanBus();
  }
  // if there is at least one DS detected
  if (owb && devices[0]) {
    ds18b20_convert_all(owb);
    lastMeasurementRequestTimeMs = millis();
    for (int i = 0; i < MAX_DEVICES; i++) {
      deviceTempRead[i] = false;
    }
  }
}

void Supla::Sensor::OneWireBus::scanBus() {
  SUPLA_LOG_DEBUG("Searching OneWire devices:");
  OneWireBus_ROMCode deviceRomCodes[MAX_DEVICES] = {};
  int deviceCount = 0;

  int configuredDeviceCount = 0;
  while (devices[configuredDeviceCount]) {
    configuredDeviceCount++;
  }

  bool found = false;
  OneWireBus_SearchState searchState = {};
  owb_search_first(owb, &searchState, &found);
  while (found) {
    bool alreadyAdded = false;
    for (int i = 0; i < configuredDeviceCount; i++) {
      if (memcmp(devices[i]->rom_code.bytes,
                 searchState.rom_code.bytes,
                 SUPLA_DS_ADDRESS_SIZE) == 0) {
        alreadyAdded = true;
        break;
      }
    }
    if (!alreadyAdded) {
      char romCodeStr[17];
      owb_string_from_rom_code(
          searchState.rom_code, romCodeStr, sizeof(romCodeStr));
      SUPLA_LOG_DEBUG("  %d : %s", deviceCount, romCodeStr);
      deviceRomCodes[deviceCount] = searchState.rom_code;
      ++deviceCount;
    }
    owb_search_next(owb, &searchState, &found);
    delay(0);
  }
  SUPLA_LOG_DEBUG(
      "Found %d new device%s", deviceCount, deviceCount == 1 ? "" : "s");

  for (int i = 0; i < deviceCount; ++i) {
    devices[configuredDeviceCount + i] = ds18b20_malloc();
    ds18b20_init(devices[configuredDeviceCount + i], owb, deviceRomCodes[i]);
    ds18b20_use_crc(devices[configuredDeviceCount + i], true);
    ds18b20_set_resolution(devices[configuredDeviceCount + i],
                           DS18B20_RESOLUTION);
    delay(0);
  }

  if (configuredDeviceCount == 0 && deviceCount) {
    // configure parasiticPower when first device is found
    bool parasiticPower = false;
    ds18b20_check_for_parasite_power(owb, &parasiticPower);
    if (parasiticPower) {
      SUPLA_LOG_DEBUG("Parasitic-powered devices detected");
    }

    owb_use_parasitic_power(owb, parasiticPower);
  }

  // DS temperature conversion take at most 750 ms with 12 bit resolution.
  // Lower resolutions uses 2 times less time on each resolution bit decrease.
  const int maxConversionTimeMs = 750;
  int divisor = 1 << (DS18B20_RESOLUTION_12_BIT - DS18B20_RESOLUTION);

  conversionTimeMs =
      maxConversionTimeMs / divisor + 50;  // add 50 ms just in case
}

bool Supla::Sensor::OneWireBus::isMeasurementReady() {
  if (lastMeasurementRequestTimeMs &&
      millis() - lastMeasurementRequestTimeMs > conversionTimeMs) {
    return true;
  }
  return false;
}

int Supla::Sensor::OneWireBus::getIndex(const uint8_t *deviceAddress) {
  if (deviceAddress[0]) {
    for (int i = 0; i < MAX_DEVICES && devices[i]; i++) {
      bool found = false;
      for (int j = 0; j < SUPLA_DS_ADDRESS_SIZE; j++) {
        if (deviceAddress[j] !=
            devices[i]->rom_code.bytes[SUPLA_DS_ADDRESS_SIZE - 1 - j]) {
          break;
        }
        if (j == SUPLA_DS_ADDRESS_SIZE - 1) {
          found = true;
        }
      }
      if (found) {
        return i;
      }
    }
  } else {
    return -1;
  }
  // not found
  return -1;
}

bool Supla::Sensor::OneWireBus::isAlreadyRead(const uint8_t *deviceAddress) {
  int index = getIndex(deviceAddress);
  if (index >= 0 && index < MAX_DEVICES) {
    return deviceTempRead[index];
  }
  return false;
}

double Supla::Sensor::OneWireBus::getTemp(const uint8_t *deviceAddress) {
  int index = getIndex(deviceAddress);

  if (index >= 0 && index < MAX_DEVICES && devices[index]) {
    deviceTempRead[index] = true;
    float reading = TEMPERATURE_NOT_AVAILABLE;
    DS18B20_ERROR error = ds18b20_read_temp(devices[index], &reading);

    if (error == DS18B20_OK) {
      return reading;
    }
  }
  return TEMPERATURE_NOT_AVAILABLE;
}

Supla::Sensor::DS18B20::DS18B20(uint8_t gpio, uint8_t *deviceAddress) {
  Supla::Sensor::OneWireBus *bus = firstOneWireBus;
  Supla::Sensor::OneWireBus *prevBus = nullptr;

  if (bus) {
    while (bus) {
      if (bus->gpio == gpio) {
        myBus = bus;
        break;
      }
      prevBus = bus;
      bus = bus->nextBus;
    }
  }

  // There is no OneWire bus created yet for this gpio
  if (!bus) {
    SUPLA_LOG_DEBUG("Creating OneWire bus for GPIO: %d", gpio);
    myBus = new OneWireBus(gpio);
    if (prevBus) {
      prevBus->nextBus = myBus;
    } else {
      firstOneWireBus = myBus;
    }
  }

  myBus->addDsToList(this);

  if (deviceAddress) {
    memcpy(address, deviceAddress, sizeof(address));
  }
}

void Supla::Sensor::OneWireBus::addDsToList(Supla::Sensor::DS18B20 *ptr) {
  auto currentPtr = firstDs;

  while (currentPtr && currentPtr->nextDs) {
    currentPtr = currentPtr->nextDs;
  }

  if (currentPtr) {
    currentPtr->nextDs = ptr;
  } else {
    firstDs = ptr;
  }
}

void Supla::Sensor::DS18B20::iterateAlways() {
  if (!myBus->lastReadTime ||
      millis() - myBus->lastReadTime > refreshIntervalMs) {
    myBus->requestTemperatures();
    myBus->lastReadTime = millis();
  }
  if (myBus->isMeasurementReady() && !myBus->isAlreadyRead(address)) {
    // is current DS channel doesn't have device assigned yet, try to
    // obtain address from OneWire bus
    assignAddressIfNeeded();
    channel.setNewValue(getValue());
  }
}

double Supla::Sensor::DS18B20::getValue() {
  double value = TEMPERATURE_NOT_AVAILABLE;
  if (address[0]) {
    if (myBus) {
      value = myBus->getTemp(address);
    }
    if (value <= TEMPERATURE_NOT_AVAILABLE || value == 85.0) {
      retryCounter++;
      if (retryCounter > 3) {
        return TEMPERATURE_NOT_AVAILABLE;
      }
      value = lastValidValue;
    } else {
      lastValidValue = value;
      retryCounter = 0;
    }
  }
  return value;
}

void Supla::Sensor::DS18B20::clearAssignedAddress(uint8_t *deviceAddress) {
  auto bus = firstOneWireBus;

  while (bus && !bus->removeDs(deviceAddress)) {
    bus = bus->nextBus;
  }
}

bool Supla::Sensor::OneWireBus::removeDs(uint8_t *address) {
  // find device
  int index = getIndex(address);

  if (index >= 0) {
    // remove from devices array
    ds18b20_free(&devices[index]);

    int lastIndex = MAX_DEVICES - 1;
    while (lastIndex > index && !devices[lastIndex]) {
      lastIndex--;
    }

    devices[index] = devices[lastIndex];
    devices[lastIndex] = nullptr;

    // remove address from DS channel
    auto dsPtr = firstDs;
    while (dsPtr) {
      if (memcmp(address, dsPtr->address, SUPLA_DS_ADDRESS_SIZE) == 0) {
        dsPtr->releaseAddress();
        break;
      }
      dsPtr = dsPtr->nextDs;
    }
    return true;
  }

  return false;
}

void Supla::Sensor::DS18B20::releaseAddress() {
  if (address[0] != 0) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
      generateKey(key, "address");
      memset(address, 0, sizeof(address));
      if (!cfg->setBlob(
              key, reinterpret_cast<char *>(address), SUPLA_DS_ADDRESS_SIZE)) {
        SUPLA_LOG_WARNING("Failed to write DS address to config");
      }
      cfg->saveWithDelay(2000);
    }
  }
}

Supla::Sensor::OneWireBus *Supla::Sensor::DS18B20::firstOneWireBus = nullptr;
