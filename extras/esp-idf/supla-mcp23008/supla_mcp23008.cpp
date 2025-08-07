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

#include "supla_mcp23008.h"

#include <supla/log_wrapper.h>

using Supla::MCP23008;

MCP23008::MCP23008(Supla::I2CDriver *driver, uint8_t address, bool initDefaults)
    : Supla::Io::Base(false),
      driver(driver),
      address(address),
      initDefaults(initDefaults) {
}

void MCP23008::customPinMode(int channelNumber, uint8_t pin, uint8_t mode) {
  if (pin > 7) {
    return;
  }
  if (handle == nullptr) {
    return;
  }
  SUPLA_LOG_DEBUG(" *** GPIO %d set mode %d", pin, mode);

  switch (mode) {
    case INPUT: {
      mode |= (1 << pin);
      pullup &= ~(1 << pin);
      break;
    }
    case OUTPUT: {
      mode &= ~(1 << pin);
      break;
    }
    case INPUT_PULLUP: {
      mode |= (1 << pin);
      pullup |= (1 << pin);
      break;
    }
    default: {
      SUPLA_LOG_ERROR("GPIO pinMode: mode %d is not implemented", mode);
      break;
    }
  }
  writeMode();
  readState();
}

int MCP23008::customDigitalRead(int channelNumber, uint8_t pin) {
  readState();
  return (state & (1 << pin)) > 0 ? 1 : 0;
}

void MCP23008::customDigitalWrite(int channelNumber, uint8_t pin, uint8_t val) {
  if (pin > 7) {
    return;
  }
  if (handle == nullptr) {
    return;
  }
  if (val) {
    state |= (1 << pin);
  } else {
    state &= ~(1 << pin);
  }
  writeState();
}

void MCP23008::writeMode() {
  driver->aquire();
  uint8_t data[2] = {static_cast<uint8_t>(Register::IODIR), mode};
  i2c_master_transmit(handle, data, 2, 400);
  data[0] = static_cast<uint8_t>(Register::GPPU);
  data[1] = pullup;
  i2c_master_transmit(handle, data, 2, 400);
  driver->release();
}

void MCP23008::writeState() {
  driver->aquire();
  uint8_t data[2] = {static_cast<uint8_t>(Register::GPIO), state};
  i2c_master_transmit(handle, data, 2, 400);
  driver->release();
}

void MCP23008::readState() {
  driver->aquire();
  uint8_t addr = static_cast<uint8_t>(Register::GPIO);
  i2c_master_transmit_receive(handle, &addr, 1, &state, 1, 400);
  driver->release();
}

unsigned int MCP23008::customPulseIn(int channelNumber,
                                     uint8_t pin,
                                     uint8_t value,
                                     uint64_t timeoutMicro) {
  SUPLA_LOG_WARNING("MCP23008: pulseIn not supported");
  return 0;
}

void MCP23008::customAnalogWrite(int channelNumber, uint8_t pin, int val) {
  SUPLA_LOG_WARNING("MCP23008: analog write not supported");
}

int MCP23008::customAnalogRead(int channelNumber, uint8_t pin) {
  SUPLA_LOG_WARNING("MCP23008: analog read not supported");
  return 0;
}

bool MCP23008::init() {
  SUPLA_LOG_DEBUG("MCP23008 init, address %02X", address);
  if (driver) {
    if (!driver->isInitialized()) {
      driver->initialize();
    }

    handle = driver->addDevice(address, 100000);

    if (handle == nullptr) {
      return false;
    }

    SUPLA_LOG_DEBUG("MCP23008: initial state:");
    readAllAndPrint();

    if (initDefaults) {
      driver->aquire();
      uint8_t buf[2] = {static_cast<uint8_t>(Register::IODIR), 0xFF};
      i2c_master_transmit(handle, buf, 2, 300);
      for (uint8_t addr = 1; addr < 8; addr++) {
        buf[0] = addr;
        buf[1] = 0;
        i2c_master_transmit(handle, buf, 2, 300);
      }
      driver->release();
    } else {
      driver->aquire();
      // read state related regiesters
      uint8_t addr = static_cast<uint8_t>(Register::IODIR);
      i2c_master_transmit_receive(handle, &addr, 1, &mode, 1, 300);

      addr = static_cast<uint8_t>(Register::GPIO);
      i2c_master_transmit_receive(handle, &addr, 1, &state, 1, 300);

      addr = static_cast<uint8_t>(Register::GPPU);
      i2c_master_transmit_receive(handle, &addr, 1, &pullup, 1, 300);

      uint8_t buf[2] = {static_cast<uint8_t>(Register::IODIR), 0};
      i2c_master_transmit(handle, buf, 2, 300);
      driver->release();
    }
    SUPLA_LOG_DEBUG("MCP23008: state after init:");
    readAllAndPrint();
  }
  return true;
}

void MCP23008::readAllAndPrint() {
  if (handle == nullptr) {
    return;
  }
  driver->aquire();
  char text[8 * 3 + 1] = {};
  for (int i = 0; i < 8; i++) {
    uint8_t buf = 0;
    uint8_t addr = i;
    i2c_master_transmit_receive(handle, &addr, 1, &buf, 1, 300);
    snprintf(text + i * 3, 4, " %02X", buf);  // NOLINT(runtime/printf)
  }
  SUPLA_LOG_DEBUG("MCP23008: %s", text);
  driver->release();
}
