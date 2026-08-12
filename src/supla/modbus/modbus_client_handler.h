// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
