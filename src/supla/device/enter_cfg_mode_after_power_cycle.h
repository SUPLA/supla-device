// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
  void resetCounter();

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
