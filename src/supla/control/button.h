// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_BUTTON_H_
#define SRC_SUPLA_CONTROL_BUTTON_H_

#include <stdint.h>
#include <supla/action_handler.h>
#include "action_trigger.h"
#include "simple_button.h"

class SuplaDeviceClass;

namespace Supla {
namespace Io {
class Base;
}  // namespace Io

namespace Control {

class Button : public SimpleButton, public ActionHandler {
 public:
  friend class ActionTrigger;
  enum class ButtonType : uint8_t {
    MONOSTABLE,
    BISTABLE,
    MOTION_SENSOR,
    CENTRAL_CONTROL
  };

  enum class OnLoadConfigType : uint8_t {
    LOAD_FULL_CONFIG,
    LOAD_BUTTON_SETUP_ONLY,
    DONT_LOAD_CONFIG
  };

  explicit Button(Supla::Io::IoPin inputPin);
  explicit Button(Supla::Io::Base *io,
                  int pin,
                  bool pullUp = false,
                  bool invertLogic = false);
  explicit Button(int pin, bool pullUp = false, bool invertLogic = false);

  void onTimer() override;
  void onLoadConfig(SuplaDeviceClass *) override;
  void onInit() override;
  void addAction(uint16_t action, ActionHandler &client, uint16_t event,
      bool alwaysEnabled = false) override;
  void addAction(uint16_t action, ActionHandler *client, uint16_t event,
      bool alwaysEnabled = false) override;
  void disableAction(int32_t action,
                     ActionHandler *client,
                     int32_t event) override;
  void enableAction(int32_t action,
                    ActionHandler *client,
                    int32_t event) override;

  void setHoldTime(unsigned int timeMs);
  void repeatOnHoldEvery(unsigned int timeMs);

  // setting of bistableButton is for backward compatiblity.
  // Use setButtonType instaed.
  void setMulticlickTime(unsigned int timeMs, bool bistableButton = false);

  void setButtonType(const ButtonType type);
  bool isBistable() const;
  bool isMonostable() const;
  bool isMotionSensor() const;
  bool isCentral() const;

  virtual void configureAsConfigButton(SuplaDeviceClass *sdc);
  bool disableActionsInConfigMode() override;
  void dontUseOnLoadConfig();
  void setOnLoadConfigType(OnLoadConfigType type);

  uint8_t getMaxMulticlickValue();
  int8_t getButtonNumber() const override;
  void setButtonNumber(int8_t number);

  void handleAction(int event, int action) override;

  void disableButton();
  void enableButton();
  void waitForRelease();

  uint32_t getLastStateChange() const;

  void setAllowHoldOnPowerOn(bool allow) {
    runtimeFlags.allowHoldOnPowerOn = allow;
  }

 protected:
  struct RuntimeFlags {
    uint16_t actionTriggerModeLocked : 1;
    uint16_t actionTriggerLocalUnlockAllowed : 1;
    uint16_t keepConfigButtonTriggerAlwaysAvailable : 1;
    uint16_t actionTriggerSuppressActionsUntilRelease : 1;
    uint16_t actionTriggerDispatching : 1;
    uint16_t suppressActionsUntilRelease : 1;
    uint16_t repeatOnHoldEnabled : 1;
    uint16_t configButton : 1;
    uint16_t disabled : 1;
    uint16_t allowHoldOnPowerOn : 1;
    uint16_t waitingForRelease : 1;
    uint16_t conditionalActionsOnClick1 : 1;
    uint16_t reserved : 4;
  };

  static_assert(sizeof(RuntimeFlags) == sizeof(uint16_t),
                "Button runtime flags must fit in uint16_t");

  void setActionTriggerModeLocked(bool locked);
  void setActionTriggerModeLocked(bool locked,
                                  bool localUnlockAllowed,
                                  bool keepConfigButtonTriggerAlwaysAvailable);
  bool runActionWithActionTriggerPolicy(uint16_t event);
  void evaluateMaxMulticlickValue();
  // Used by ActionTrigger for bistable directional press/release pairs.
  // Counting clicks (including CFG x10) remains independent of this policy.
  // Reapplying the same policy preserves pending clicks and release timing.
  void setConditionalActionsOnClick1(bool enabled);
  // disbles repeating "on hold" if repeat time is lower than threshold
  // threshold 0 disables always
  void disableRepeatOnHold(uint32_t threshold = 0);
  void enableRepeatOnHold();
  const char *getButtonTypeName(ButtonType type) const;
  uint32_t lastStateChangeMs = 0;
  uint16_t repeatOnHoldMs = 0;
  uint16_t holdSend = 0;
  uint16_t holdTimeMs = 0;
  uint16_t multiclickTimeMs = 0;
  RuntimeFlags runtimeFlags = {};
  ButtonType buttonType = ButtonType::MONOSTABLE;
  enum OnLoadConfigType onLoadConfigType = OnLoadConfigType::LOAD_FULL_CONFIG;

  uint8_t clickCounter = 0;
  uint8_t maxMulticlickValueConfigured = 0;
  int8_t buttonNumber = -1;

  static int buttonCounter;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BUTTON_H_
