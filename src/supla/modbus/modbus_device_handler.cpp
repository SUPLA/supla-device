// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "modbus_device_handler.h"

#include <supla/log_wrapper.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/device/register_device.h>
#include <string.h>

using Supla::ModbusDeviceHandler;

#define DEVICE_REGISTER_BLOCK_MAX_SIZE 100

#define DEVICE_NAME_REGISTER 0
#define DEVICE_NAME_SIZE_BYTES 32
#define DEVICE_SW_VERSION_REGISTER \
  (DEVICE_NAME_REGISTER + DEVICE_NAME_SIZE_BYTES / 2)
#define DEVICE_SW_VERSION_SIZE_BYTES 22
#define DEVICE_GUID_REGISTER \
  (DEVICE_SW_VERSION_REGISTER + DEVICE_SW_VERSION_SIZE_BYTES / 2)
#define DEVICE_GUID_SIZE_BYTES 38

static_assert(DEVICE_NAME_SIZE_BYTES % 2 == 0);
static_assert(DEVICE_SW_VERSION_SIZE_BYTES % 2 == 0);
static_assert(DEVICE_GUID_SIZE_BYTES % 2 == 0);

static_assert((DEVICE_GUID_REGISTER + DEVICE_GUID_SIZE_BYTES / 2) <
              DEVICE_REGISTER_BLOCK_MAX_SIZE);

ModbusDeviceHandler::ModbusDeviceHandler(uint16_t offset) {
  modbusAddressOffset = offset;
  usedRegistersCount = DEVICE_REGISTER_BLOCK_MAX_SIZE;
}

Supla::Modbus::Result ModbusDeviceHandler::holdingProcessRequest(
    uint16_t address,
    uint16_t nRegs,
    uint8_t *regBuffer,
    Supla::Modbus::Access access) {
  auto localAddress = address - modbusAddressOffset;

  if (access == Supla::Modbus::Access::READ) {
    memset(regBuffer, 0, nRegs * 2);
    for (int reg = 0; reg < nRegs; reg++) {
      auto currentAddress = (localAddress + reg);

      if (currentAddress >= DEVICE_NAME_REGISTER &&
          currentAddress < DEVICE_NAME_REGISTER + DEVICE_NAME_SIZE_BYTES / 2) {
        // device name
        auto name = Supla::RegisterDevice::getName();
        char buf[DEVICE_NAME_SIZE_BYTES] = {};
        strncpy(buf, name, DEVICE_NAME_SIZE_BYTES - 1);

        int regOffset = currentAddress - DEVICE_NAME_REGISTER;

        regBuffer += fillRegBuffer(
            regBuffer, buf, &reg, regOffset, nRegs, DEVICE_NAME_SIZE_BYTES);

        continue;
      }
      if (currentAddress >= DEVICE_SW_VERSION_REGISTER &&
          currentAddress <
              DEVICE_SW_VERSION_REGISTER + DEVICE_SW_VERSION_SIZE_BYTES / 2) {
        // device software version
        auto version = Supla::RegisterDevice::getSoftVer();
        char buf[DEVICE_SW_VERSION_SIZE_BYTES] = {};
        strncpy(buf, version, DEVICE_SW_VERSION_SIZE_BYTES - 1);

        int regOffset = currentAddress - DEVICE_SW_VERSION_REGISTER;

        regBuffer += fillRegBuffer(regBuffer,
                                   buf,
                                   &reg,
                                   regOffset,
                                   nRegs,
                                   DEVICE_SW_VERSION_SIZE_BYTES);
        continue;
      }

      if (currentAddress >= DEVICE_GUID_REGISTER &&
          currentAddress < DEVICE_GUID_REGISTER + DEVICE_GUID_SIZE_BYTES / 2) {
        // device GUID
        char buf[DEVICE_GUID_SIZE_BYTES] = {};
        Supla::RegisterDevice::fillGUIDText(buf);

        int regOffset = currentAddress - DEVICE_GUID_REGISTER;
        int regSize = nRegs - reg;
        if (regSize > (DEVICE_GUID_SIZE_BYTES / 2) - regOffset) {
          regSize = (DEVICE_GUID_SIZE_BYTES / 2) - regOffset;
        }

        int byteOffset = regOffset * 2;
        int remainingBytes = regSize * 2;

        memcpy(regBuffer, buf + byteOffset, remainingBytes);
        regBuffer += remainingBytes;
        reg += regSize - 1;
        continue;
      }

      return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
      }
    return Supla::Modbus::Result::OK;
  }

  return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
}

bool ModbusDeviceHandler::isHoldingSupported() {
  return true;
}

bool ModbusDeviceHandler::isInputSupported() {
  return true;
}

int ModbusDeviceHandler::fillRegBuffer(uint8_t *regBuffer,
                                       char *input,
                                       int *reg,
                                       int regOffset,
                                       int nRegs,
                                       int fieldSize) {
  int regSize = nRegs - (*reg);

  if (regSize > (fieldSize / 2) - regOffset) {
    regSize = (fieldSize / 2) - regOffset;
  }

  int byteOffset = regOffset * 2;
  int remainingBytes = regSize * 2;

  memcpy(regBuffer, input + byteOffset, remainingBytes);
  *reg += regSize - 1;
  return remainingBytes;
}
