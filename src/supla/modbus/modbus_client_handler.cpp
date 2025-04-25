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

#include "modbus_client_handler.h"

#include <supla/log_wrapper.h>

using Supla::ModbusClientHandler;


ModbusClientHandler::ModbusClientHandler() {
  if (first == nullptr) {
    first = this;
  } else {
    ModbusClientHandler *handler = first;
    while (handler->next != nullptr) {
      handler = handler->next;
    }
    handler->next = this;
  }
}

ModbusClientHandler::~ModbusClientHandler() {
  if (first == this) {
    first = next;
  } else {
    ModbusClientHandler *handler = first;
    while (handler != nullptr) {
      if (handler->next == this) {
        handler->next = next;
        break;
      }
      handler = handler->next;
    }
  }
}

Supla::Modbus::Result ModbusClientHandler::HoldingProcessRequest(
    uint16_t address,
    uint16_t nRegs,
    uint8_t *regBuffer,
    Supla::Modbus::Access access) {
  auto handler = GetHoldingHandler(address, nRegs);
  if (handler != nullptr) {
    return handler->holdingProcessRequest(address, nRegs, regBuffer, access);
  }
  return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
}

Supla::Modbus::Result ModbusClientHandler::InputProcessRequest(uint16_t address,
                                                       uint16_t nRegs,
                                                       uint8_t *regBuffer) {
  auto handler = GetInputHandler(address, nRegs);
  if (handler != nullptr) {
    return handler->inputProcessRequest(address, nRegs, regBuffer);
  }
  return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
}

Supla::Modbus::Result ModbusClientHandler::DiscreteProcessRequest(
    uint16_t address, uint16_t nRegs, uint8_t *regBuffer) {
  auto handler = GetDiscreteHandler(address, nRegs);
  if (handler != nullptr) {
    return handler->discreteProcessRequest(address, nRegs, regBuffer);
  }
  return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
}

Supla::Modbus::Result ModbusClientHandler::CoilsProcessRequest(
    uint16_t address,
    uint16_t nRegs,
    uint8_t *regBuffer,
    Supla::Modbus::Access access) {
  auto handler = GetCoilsHandler(address, nRegs);
  if (handler != nullptr) {
    return handler->coilsProcessRequest(address, nRegs, regBuffer, access);
  }
  return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
}

ModbusClientHandler *ModbusClientHandler::GetHoldingHandler(uint16_t address,
                                                            uint16_t nRegs) {
  ModbusClientHandler *handler = first;
  while (handler != nullptr) {
    if (handler->isHoldingSupported() &&
        handler->holdingRespondsToAddress(address, nRegs)) {
      return handler;
    }
    handler = handler->next;
  }
  SUPLA_LOG_WARNING("ModbusClientHandler: no handler for address 0x%04X",
                    address);
  return nullptr;
}

ModbusClientHandler *ModbusClientHandler::GetInputHandler(uint16_t address,
                                                          uint16_t nRegs) {
  ModbusClientHandler *handler = first;
  while (handler != nullptr) {
    if (handler->isInputSupported() &&
        handler->inputRespondsToAddress(address, nRegs)) {
      return handler;
    }
    handler = handler->next;
  }
  SUPLA_LOG_WARNING("ModbusClientHandler: no handler for address 0x%04X",
                    address);
  return nullptr;
}

ModbusClientHandler *ModbusClientHandler::GetDiscreteHandler(uint16_t address,
                                                             uint16_t nRegs) {
  ModbusClientHandler *handler = first;
  while (handler != nullptr) {
    if (handler->isDiscreteSupported() &&
        handler->discreteRespondsToAddress(address, nRegs)) {
      return handler;
    }
    handler = handler->next;
  }
  SUPLA_LOG_WARNING("ModbusClientHandler: no handler for address 0x%04X",
                    address);
  return nullptr;
}

ModbusClientHandler *ModbusClientHandler::GetCoilsHandler(uint16_t address,
                                                          uint16_t nRegs) {
  ModbusClientHandler *handler = first;
  while (handler != nullptr) {
    if (handler->isCoilsSupported() &&
        handler->coilsRespondsToAddress(address, nRegs)) {
      return handler;
    }
    handler = handler->next;
  }
  SUPLA_LOG_WARNING("ModbusClientHandler: no handler for address 0x%04X",
                    address);
  return nullptr;
}

bool ModbusClientHandler::isInputSupported() {
  return false;
}

bool ModbusClientHandler::isDiscreteSupported() {
  return false;
}

bool ModbusClientHandler::isCoilsSupported() {
  return false;
}

bool ModbusClientHandler::isHoldingSupported() {
  return false;
}

bool ModbusClientHandler::IsInputSupported() {
  ModbusClientHandler *handler = first;
  while (handler != nullptr) {
    if (handler->isInputSupported()) {
      return true;
    }
    handler = handler->next;
  }
  return false;
}

bool ModbusClientHandler::IsDiscreteSupported() {
  ModbusClientHandler *handler = first;
  while (handler != nullptr) {
    if (handler->isDiscreteSupported()) {
      return true;
    }
    handler = handler->next;
  }
  return false;
}

bool ModbusClientHandler::IsCoilsSupported() {
  ModbusClientHandler *handler = first;
  while (handler != nullptr) {
    if (handler->isCoilsSupported()) {
      return true;
    }
    handler = handler->next;
  }
  return false;
}

bool ModbusClientHandler::IsHoldingSupported() {
  ModbusClientHandler *handler = first;
  while (handler != nullptr) {
    if (handler->isHoldingSupported()) {
      return true;
    }
    handler = handler->next;
  }
  return false;
}

Supla::Modbus::Result ModbusClientHandler::holdingProcessRequest(
    uint16_t address,
    uint16_t nRegs,
    uint8_t *regBuffer,
    Supla::Modbus::Access access) {
  (void)(address);
  (void)(nRegs);
  (void)(regBuffer);
  (void)(access);
  return Supla::Modbus::Result::INVALID_STATE;
}

Supla::Modbus::Result ModbusClientHandler::inputProcessRequest(uint16_t address,
                                                   uint16_t nRegs,
                                                   uint8_t *regBuffer) {
  if (isInputSupported()) {
    return holdingProcessRequest(
        address, nRegs, regBuffer, Supla::Modbus::Access::READ);
  }
  return Supla::Modbus::Result::INVALID_STATE;
}


bool ModbusClientHandler::coilsRespondsToAddress(uint16_t address,
                                                 uint16_t nRegs) {
  (void)(address);
  (void)(nRegs);
  return false;
}

Supla::Modbus::Result ModbusClientHandler::coilsProcessRequest(
    uint16_t address,
    uint16_t nRegs,
    uint8_t *regBuffer,
    Supla::Modbus::Access access) {
  (void)(address);
  (void)(nRegs);
  (void)(regBuffer);
  (void)(access);
  return Supla::Modbus::Result::INVALID_STATE;
}

bool ModbusClientHandler::discreteRespondsToAddress(uint16_t address,
                                                    uint16_t nRegs) {
  (void)(address);
  (void)(nRegs);
  return false;
}

Supla::Modbus::Result ModbusClientHandler::discreteProcessRequest(
    uint16_t address, uint16_t nRegs, uint8_t *regBuffer) {
  (void)(address);
  (void)(nRegs);
  (void)(regBuffer);
  return Supla::Modbus::Result::INVALID_STATE;
}

void ModbusClientHandler::storeBigEndian(uint64_t value,
                                     uint8_t *regBuffer,
                                     uint8_t registerOffset,
                                     uint8_t registerCount) {
  if (registerOffset > 3 || registerCount < 1 || registerCount > 4) {
    return;
  }

  for (int i = 0; i < registerCount; ++i) {
    uint16_t byteOffset = (registerOffset + i) * 2 * 8;

    uint16_t registerValue = (value >> (48 - byteOffset)) & 0xFFFF;

    regBuffer[2 * i] = (registerValue >> 8) & 0xFF;  // Most significant byte
    regBuffer[2 * i + 1] = registerValue & 0xFF;     // Least significant byte
  }
}

bool ModbusClientHandler::holdingRespondsToAddress(uint16_t address,
                                               uint16_t nRegs) {
  if (isHoldingSupported()) {
    SUPLA_LOG_DEBUG("ModbusClient: address: %d, nRegs: %d", address, nRegs);
    if (address >= modbusAddressOffset &&
        address + nRegs <= modbusAddressOffset + usedRegistersCount) {
      return true;
    }
  }
  return false;
}

bool ModbusClientHandler::inputRespondsToAddress(uint16_t address,
                                                 uint16_t nRegs) {
  return holdingRespondsToAddress(address, nRegs);
}

ModbusClientHandler *ModbusClientHandler::first = nullptr;
