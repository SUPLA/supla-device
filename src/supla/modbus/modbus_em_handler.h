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
