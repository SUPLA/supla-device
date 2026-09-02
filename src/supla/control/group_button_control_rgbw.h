// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_GROUP_BUTTON_CONTROL_RGBW_H_
#define SRC_SUPLA_CONTROL_GROUP_BUTTON_CONTROL_RGBW_H_

#include <supla/action_handler.h>
#include <supla/element.h>
#include <supla/control/lighting_pwm_base.h>

#define SUPLA_MAX_GROUP_CONTROL_ELEMENTS 10

namespace Supla::Control {

class Button;
class LightingPwmBase;

class GroupButtonControlRgbw : public ActionHandler, public Element {
 public:
  explicit GroupButtonControlRgbw(Button *button = nullptr);
  void attach(Button *button);
  void addToGroup(LightingPwmBase *rgbwElement);

  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void onInit() override;
  void handleAction(int event, int action) override;

  void setButtonControlType(int rgbwChannelNumber,
                            LightingPwmBase::ButtonControlType type);

 private:
  void handleTurnOn();
  void handleTurnOff();
  void handleToggle();
  void handleIterate();

  Button *attachedButton = nullptr;
  LightingPwmBase *rgbw[SUPLA_MAX_GROUP_CONTROL_ELEMENTS] = {};
  LightingPwmBase::ButtonControlType
      controlType[SUPLA_MAX_GROUP_CONTROL_ELEMENTS] =
      {};
  int rgbwCount = 0;
};

}  // namespace Supla::Control

#endif  // SRC_SUPLA_CONTROL_GROUP_BUTTON_CONTROL_RGBW_H_
