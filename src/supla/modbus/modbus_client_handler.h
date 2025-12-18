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

#ifndef SRC_SUPLA_MODBUS_MODBUS_CLIENT_HANDLER_H_
#define SRC_SUPLA_MODBUS_MODBUS_CLIENT_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

namespace Supla {

namespace Modbus {
enum class Result {
  OK,
  INVALID_STATE,
  INVALID_REGISTER_ADDRESS,
};

enum class Access {
  READ,
  WRITE,
};
}  // namespace Modbus

class ModbusClientHandler {
 public:
  ModbusClientHandler();

  virtual ~ModbusClientHandler();
  // RW holding register
  static Supla::Modbus::Result HoldingProcessRequest(
      uint16_t address,
      uint16_t nRegs,
      uint8_t *regBuffer,
      Supla::Modbus::Access access);
  // RO input register
  static Supla::Modbus::Result InputProcessRequest(uint16_t address,
                                                   uint16_t nRegs,
                                                   uint8_t *regBuffer);
  // RW coils
  static Supla::Modbus::Result CoilsProcessRequest(
      uint16_t address,
      uint16_t nRegs,
      uint8_t *regBuffer,
      Supla::Modbus::Access access);
  // RO discrete register
  static Supla::Modbus::Result DiscreteProcessRequest(uint16_t address,
                                                      uint16_t nRegs,
                                                      uint8_t *regBuffer);

  static bool IsInputSupported();
  static bool IsDiscreteSupported();
  static bool IsCoilsSupported();
  static bool IsHoldingSupported();

  virtual bool isInputSupported();
  virtual bool isDiscreteSupported();
  virtual bool isCoilsSupported();
  virtual bool isHoldingSupported();

  virtual bool holdingRespondsToAddress(uint16_t address, uint16_t nRegs);
  virtual Supla::Modbus::Result holdingProcessRequest(
      uint16_t address,
      uint16_t nRegs,
      uint8_t *regBuffer,
      Supla::Modbus::Access access);

  virtual bool inputRespondsToAddress(uint16_t address, uint16_t nRegs);
  virtual Supla::Modbus::Result inputProcessRequest(uint16_t address,
                                                    uint16_t nRegs,
                                                    uint8_t *regBuffer);

  virtual bool coilsRespondsToAddress(uint16_t address, uint16_t nRegs);
  virtual Supla::Modbus::Result coilsProcessRequest(
      uint16_t address,
      uint16_t nRegs,
      uint8_t *regBuffer,
      Supla::Modbus::Access access);

  virtual bool discreteRespondsToAddress(uint16_t address, uint16_t nRegs);
  virtual Supla::Modbus::Result discreteProcessRequest(uint16_t address,
                                                       uint16_t nRegs,
                                                       uint8_t *regBuffer);

 protected:
  void storeBigEndian(uint64_t value,
                      uint8_t *regBuffer,
                      uint8_t registerOffset,
                      uint8_t registerCount);
  uint16_t modbusAddressOffset = 0;
  uint16_t usedRegistersCount = 0;

 private:
  static ModbusClientHandler *GetHoldingHandler(uint16_t address,
                                                uint16_t nRegs);
  static ModbusClientHandler *GetInputHandler(uint16_t address, uint16_t nRegs);
  static ModbusClientHandler *GetDiscreteHandler(uint16_t address,
                                                 uint16_t nRegs);
  static ModbusClientHandler *GetCoilsHandler(uint16_t address, uint16_t nRegs);
  static ModbusClientHandler *first;
  ModbusClientHandler *next = nullptr;
};

}  // namespace Supla
#endif  // SRC_SUPLA_MODBUS_MODBUS_CLIENT_HANDLER_H_
