// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_MODBUS_MODBUS_DEVICE_HANDLER_H_
#define SRC_SUPLA_MODBUS_MODBUS_DEVICE_HANDLER_H_

#include "modbus_client_handler.h"

#define EM_REGISTER_BLOCK_MAX_SIZE (30)

class SuplaDeviceClass;

namespace Supla {

class ModbusDeviceHandler : public ModbusClientHandler {
 public:
  explicit ModbusDeviceHandler(uint16_t offset = 0);

  Supla::Modbus::Result holdingProcessRequest(uint16_t address,
                               uint16_t nRegs,
                               uint8_t *regBuffer,
                               Supla::Modbus::Access access) override;

  bool isHoldingSupported() override;
  bool isInputSupported() override;
 private:
  int fillRegBuffer(uint8_t *regBuffer,
                    char *input,
                    int *reg,
                    int regOffset,
                    int nRegs,
                    int fieldSize);
};

}  // namespace Supla

#endif  // SRC_SUPLA_MODBUS_MODBUS_DEVICE_HANDLER_H_
