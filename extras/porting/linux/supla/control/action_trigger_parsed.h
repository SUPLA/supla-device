// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_ACTION_TRIGGER_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_ACTION_TRIGGER_PARSED_H_

#include <supla/control/action_trigger.h>

#include <optional>
#include <string>

namespace Supla {

namespace Control {

class ActionTriggerParsed : public ActionTrigger {
 public:
  explicit ActionTriggerParsed(const std::string &name);

  void activateAction(int action) override;

  void sendActionTrigger(int action);

  static constexpr uint32_t MaximumTemporaryUnlockDurationSec = 64800;
  // Applied as a local override after loading saved settings in onLoadConfig.
  bool setTemporaryUnlockDurationSec(uint32_t seconds);
  void onLoadConfig(SuplaDeviceClass *sdc) override;

 private:
  std::optional<uint32_t> temporaryUnlockDurationSec;
};

}  // namespace Control
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_ACTION_TRIGGER_PARSED_H_
