// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_MODBUS_MODBUS_EM_HANDLER_H_
#define SRC_SUPLA_MODBUS_MODBUS_EM_HANDLER_H_

#include "modbus_client_handler.h"

#define EM_REGISTER_BLOCK_MAX_SIZE (30)

namespace Supla {

namespace Sensor {
class ElectricityMeter;
}  // namespace Sensor

class ModbusEMHandler : public ModbusClientHandler {
 public:
  explicit ModbusEMHandler(Supla::Sensor::ElectricityMeter *em,
                           uint16_t offset = 0);
  Supla::Modbus::Result holdingProcessRequest(uint16_t address,
                               uint16_t nRegs,
                               uint8_t *regBuffer,
                               Supla::Modbus::Access access) override;

  bool isHoldingSupported() override;
  bool isInputSupported() override;

 private:
  Supla::Sensor::ElectricityMeter *em = nullptr;
};

}  // namespace Supla


#endif  // SRC_SUPLA_MODBUS_MODBUS_EM_HANDLER_H_
