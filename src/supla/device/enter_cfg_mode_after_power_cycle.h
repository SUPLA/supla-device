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

#ifndef SRC_SUPLA_DEVICE_ENTER_CFG_MODE_AFTER_POWER_CYCLE_H_
#define SRC_SUPLA_DEVICE_ENTER_CFG_MODE_AFTER_POWER_CYCLE_H_

#include <supla/element.h>

class SuplaDeviceClass;

namespace Supla::Device {
class EnterCfgModeAfterPowerCycle : public Supla::Element {
 public:
  explicit EnterCfgModeAfterPowerCycle(uint32_t timeoutMs = 5000,
                                      uint32_t powerCycles = 3,
                                      bool alwaysEnabled = false);

  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void iterateAlways() override;
  void setAlwaysEnabled(bool alwaysEnabled);
  void setIncrementOnlyOnPowerResetReason(bool value);

 private:
  SuplaDeviceClass *sdc = nullptr;
  uint32_t timestampMs = 0;
  uint32_t timeoutMs = 5000;
  uint32_t maxPowerCycles = 3;
  uint32_t currentPowerCycle = 0;
  bool incremented = false;
  bool enabled = false;
  bool alwaysEnabled = false;
  bool incrementOnlyOnPowerResetReason = false;
};

}  // namespace Supla::Device

#endif  // SRC_SUPLA_DEVICE_ENTER_CFG_MODE_AFTER_POWER_CYCLE_H_
