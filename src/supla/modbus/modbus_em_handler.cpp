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

#include "modbus_em_handler.h"

#include <supla/log_wrapper.h>
#include <supla/sensor/electricity_meter.h>
#include <string.h>
#include "modbus_client_handler.h"

using Supla::ModbusEMHandler;

// Voltage(1) + PhaseAngle(1) + PowerFactor(1) + Current(2) + PowerActive(2) +
// PowerReactive(2) + PowerApparent(2) + FwdEnergyActive(4) + RevEnergyActive(4)
// + FwdEnergyReactive(4) + RevEnergyReactive(4)
#define EM_PHASE_REG_VOLTAGE 0
#define EM_PHASE_REG_PHASE_ANGLE 1
#define EM_PHASE_REG_POWER_FACTOR 2
// 3 is not used
#define EM_PHASE_REG_CURRENT 4  // 32 bit
#define EM_PHASE_REG_POWER_ACTIVE 6  // 32 bit
#define EM_PHASE_REG_POWER_REACTIVE 8  // 32 bit
#define EM_PHASE_REG_POWER_APPARENT 10  // 32 bit
#define EM_PHASE_REG_FWD_ENERGY_ACTIVE 12  // 64 bit
#define EM_PHASE_REG_RVR_ENERGY_ACTIVE 16  // 64 bit
#define EM_PHASE_REG_FWD_ENERGY_REACTIVE 20  // 64 bit
#define EM_PHASE_REG_RVR_ENERGY_REACTIVE 24  // 64 bit

// Frequency(1) + VoltagePhaseSequence(1) + CurrentPhaseSequence(1) +
// VoltagePhaseAngle12(1) + VoltagePhaseAngle13(1) +
// ForwardActiveEnergyVectorBalance(4) + ReverseActiveEnergyVectorBalance(4) =
// 13
#define EM_REGISTER_COUNT_COMMON (13)
#define EM_COMMON_REG_FREQUENCY 0
#define EM_COMMON_REG_VOLTAGE_PHASE_SEQUENCE 1
#define EM_COMMON_REG_CURRENT_PHASE_SEQUENCE 2
#define EM_COMMON_REG_VOLTAGE_PHASE_ANGLE_12 3
#define EM_COMMON_REG_VOLTAGE_PHASE_ANGLE_13 4
// 5..9 is not used
#define EM_COMMON_REG_FORWARD_ACTIVE_ENERGY_VECTOR_BALANCE 10  // 64 bit
#define EM_COMMON_REG_REVERSE_ACTIVE_ENERGY_VECTOR_BALANCE 14  // 64 bit

ModbusEMHandler::ModbusEMHandler(Supla::Sensor::ElectricityMeter *em,
                                 uint16_t offset)
    : em(em) {
  modbusAddressOffset = offset;
  usedRegistersCount = 4 * EM_REGISTER_BLOCK_MAX_SIZE;
}

Supla::Modbus::Result ModbusEMHandler::holdingProcessRequest(uint16_t address,
                                              uint16_t nRegs,
                                              uint8_t *regBuffer,
                                              Supla::Modbus::Access access) {
  if (em == nullptr) {
    return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
  }

  auto localAddress = address - modbusAddressOffset;

  if (access == Supla::Modbus::Access::READ) {
    memset(regBuffer, 0, nRegs * 2);
    for (int reg = 0; reg < nRegs; reg++) {
      // block 0 -> common
      // block 1 -> phase 1
      // block 2 -> phase 2
      // block 3 -> phase 3
      auto registersBlock = (localAddress + reg) / EM_REGISTER_BLOCK_MAX_SIZE;
      auto addressInsideBlock =
          (localAddress + reg) % EM_REGISTER_BLOCK_MAX_SIZE;

      if (registersBlock == 0) {
        switch (addressInsideBlock) {
          case EM_COMMON_REG_FREQUENCY: {
            auto freq = em->getFreq();
            // store in big endian format
            *(regBuffer++) = freq >> 8;
            *(regBuffer++) = freq & 0xFF;
            break;
          }
          case EM_COMMON_REG_VOLTAGE_PHASE_SEQUENCE: {
            if (em->isVoltagePhaseSequenceSet()) {
              uint8_t value = 1;
              if (em->isVoltagePhaseSequenceClockwise()) {
                value = 2;
              }
              regBuffer++;  // empty byte
              *(regBuffer++) = value;
            }
            break;
          }
          case EM_COMMON_REG_CURRENT_PHASE_SEQUENCE: {
            if (em->isCurrentPhaseSequenceSet()) {
              uint8_t value = 1;
              if (em->isCurrentPhaseSequenceClockwise()) {
                value = 2;
              }
              regBuffer++;  // empty byte
              *(regBuffer++) = value;
            }
            break;
          }
          case EM_COMMON_REG_VOLTAGE_PHASE_ANGLE_12: {
            auto angle = em->getVoltagePhaseAngle12();
            // store in big endian format
            *(regBuffer++) = angle >> 8;
            *(regBuffer++) = angle & 0xFF;
            break;
          }
          case EM_COMMON_REG_VOLTAGE_PHASE_ANGLE_13: {
            auto angle = em->getVoltagePhaseAngle13();
            // store in big endian format
            *(regBuffer++) = angle >> 8;
            *(regBuffer++) = angle & 0xFF;
            break;
          }
          case EM_COMMON_REG_FORWARD_ACTIVE_ENERGY_VECTOR_BALANCE:
          case EM_COMMON_REG_FORWARD_ACTIVE_ENERGY_VECTOR_BALANCE + 1:
          case EM_COMMON_REG_FORWARD_ACTIVE_ENERGY_VECTOR_BALANCE + 2:
          case EM_COMMON_REG_FORWARD_ACTIVE_ENERGY_VECTOR_BALANCE + 3: {
            auto value = em->getFwdBalancedActEnergy();
            value = 0x0123456789ABCDEF;
            // store in big endian format
            int regOffset = addressInsideBlock -
                            EM_COMMON_REG_FORWARD_ACTIVE_ENERGY_VECTOR_BALANCE;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(value, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
          case EM_COMMON_REG_REVERSE_ACTIVE_ENERGY_VECTOR_BALANCE:
          case EM_COMMON_REG_REVERSE_ACTIVE_ENERGY_VECTOR_BALANCE + 1:
          case EM_COMMON_REG_REVERSE_ACTIVE_ENERGY_VECTOR_BALANCE + 2:
          case EM_COMMON_REG_REVERSE_ACTIVE_ENERGY_VECTOR_BALANCE + 3: {
            auto value = em->getRvrBalancedActEnergy();
            // store in big endian format
            int regOffset = addressInsideBlock -
                            EM_COMMON_REG_REVERSE_ACTIVE_ENERGY_VECTOR_BALANCE;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(value, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
        }
      } else if (registersBlock < 4) {
        auto phase = registersBlock - 1;
        switch (addressInsideBlock) {
          case EM_PHASE_REG_VOLTAGE: {
            auto voltage = em->getVoltage(phase);
            // store in big endian format
            *(regBuffer++) = voltage >> 8;
            *(regBuffer++) = voltage & 0xFF;
            break;
          }
          case EM_PHASE_REG_PHASE_ANGLE: {
            auto angle = em->getPhaseAngle(phase);
            // store in big endian format
            *(regBuffer++) = angle >> 8;
            *(regBuffer++) = angle & 0xFF;
            break;
          }
          case EM_PHASE_REG_POWER_FACTOR: {
            auto factor = em->getPowerFactor(phase);
            // store in big endian format
            *(regBuffer++) = factor >> 8;
            *(regBuffer++) = factor & 0xFF;
            break;
          }
          case EM_PHASE_REG_CURRENT:
          case EM_PHASE_REG_CURRENT + 1: {
            auto current = em->getCurrent(phase);
            // store in big endian format
            int regOffset = addressInsideBlock - EM_PHASE_REG_CURRENT + 2;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(current, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
          case EM_PHASE_REG_POWER_ACTIVE:
          case EM_PHASE_REG_POWER_ACTIVE + 1: {
            // active power is in int64 in 0.00001 W units
            // We will use int32 with 0.001 W units
            auto active = em->getPowerActive(phase);
            int32_t value = active / 100;
            // store in big endian format
            int regOffset = addressInsideBlock - EM_PHASE_REG_POWER_ACTIVE + 2;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(value, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
          case EM_PHASE_REG_POWER_REACTIVE:
          case EM_PHASE_REG_POWER_REACTIVE + 1: {
            // reactive power is in int64 in 0.00001 W units
            // We will use int32 with 0.001 W units
            auto reactive = em->getPowerReactive(phase);
            int32_t value = reactive / 100;
            // store in big endian format
            int regOffset =
                addressInsideBlock - EM_PHASE_REG_POWER_REACTIVE + 2;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(value, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
          case EM_PHASE_REG_POWER_APPARENT:
          case EM_PHASE_REG_POWER_APPARENT + 1: {
            // apparent power is in int64 in 0.00001 W units
            // We will use int32 with 0.001 W units
            auto apparent = em->getPowerApparent(phase);
            int32_t value = apparent / 100;
            // store in big endian format
            int regOffset =
                addressInsideBlock - EM_PHASE_REG_POWER_APPARENT + 2;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(value, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
          case EM_PHASE_REG_FWD_ENERGY_ACTIVE:
          case EM_PHASE_REG_FWD_ENERGY_ACTIVE + 1:
          case EM_PHASE_REG_FWD_ENERGY_ACTIVE + 2:
          case EM_PHASE_REG_FWD_ENERGY_ACTIVE + 3: {
            auto energy = em->getFwdActEnergy(phase);
            // store in big endian format
            int regOffset = addressInsideBlock - EM_PHASE_REG_FWD_ENERGY_ACTIVE;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(energy, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
          case EM_PHASE_REG_RVR_ENERGY_ACTIVE:
          case EM_PHASE_REG_RVR_ENERGY_ACTIVE + 1:
          case EM_PHASE_REG_RVR_ENERGY_ACTIVE + 2:
          case EM_PHASE_REG_RVR_ENERGY_ACTIVE + 3: {
            auto energy = em->getRvrActEnergy(phase);
            // store in big endian format
            int regOffset = addressInsideBlock - EM_PHASE_REG_RVR_ENERGY_ACTIVE;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(energy, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
          case EM_PHASE_REG_FWD_ENERGY_REACTIVE:
          case EM_PHASE_REG_FWD_ENERGY_REACTIVE + 1:
          case EM_PHASE_REG_FWD_ENERGY_REACTIVE + 2:
          case EM_PHASE_REG_FWD_ENERGY_REACTIVE + 3: {
            auto energy = em->getFwdReactEnergy(phase);
            // store in big endian format
            int regOffset =
                addressInsideBlock - EM_PHASE_REG_FWD_ENERGY_REACTIVE;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(energy, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
          case EM_PHASE_REG_RVR_ENERGY_REACTIVE:
          case EM_PHASE_REG_RVR_ENERGY_REACTIVE + 1:
          case EM_PHASE_REG_RVR_ENERGY_REACTIVE + 2:
          case EM_PHASE_REG_RVR_ENERGY_REACTIVE + 3: {
            auto energy = em->getRvrReactEnergy(phase);
            // store in big endian format
            int regOffset =
                addressInsideBlock - EM_PHASE_REG_RVR_ENERGY_REACTIVE;
            int size = nRegs - reg;
            if (size > 4 - regOffset) {
              size = 4 - regOffset;
            }
            storeBigEndian(energy, regBuffer, regOffset, size);
            reg += size - 1;
            regBuffer += size * 2;
            break;
          }
        }
      } else {
        return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
      }
    }
    return Supla::Modbus::Result::OK;
  }

  return Supla::Modbus::Result::INVALID_REGISTER_ADDRESS;
}

bool ModbusEMHandler::isHoldingSupported() {
  return true;
}

bool ModbusEMHandler::isInputSupported() {
  return true;
}

