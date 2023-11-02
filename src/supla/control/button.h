/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_CONTROL_BUTTON_H_
#define SRC_SUPLA_CONTROL_BUTTON_H_

#include <stdint.h>
#include "action_trigger.h"
#include "simple_button.h"

class SuplaDeviceClass;

namespace Supla {
namespace Control {

class Button : public SimpleButton {
 public:
  friend class ActionTrigger;
  enum class ButtonType {
    MONOSTABLE,
    BISTABLE,
    MOTION_SENSOR
  };

  explicit Button(Supla::Io *io,
                  int pin,
                  bool pullUp = false,
                  bool invertLogic = false);
  explicit Button(int pin, bool pullUp = false, bool invertLogic = false);

  void onTimer() override;
  void onLoadConfig(SuplaDeviceClass *) override;
  void onInit() override;
  void addAction(int action, ActionHandler &client, int event,
      bool alwaysEnabled = false) override;
  void addAction(int action, ActionHandler *client, int event,
      bool alwaysEnabled = false) override;
  void disableAction(int action, ActionHandler *client, int event) override;
  void enableAction(int action, ActionHandler *client, int event) override;

  void setHoldTime(unsigned int timeMs);
  void repeatOnHoldEvery(unsigned int timeMs);

  // setting of bistableButton is for backward compatiblity.
  // Use setButtonType instaed.
  void setMulticlickTime(unsigned int timeMs, bool bistableButton = false);

  void setButtonType(const ButtonType type);
  bool isBistable() const;
  bool isMonostable() const;
  bool isMotionSensor() const;

  virtual void configureAsConfigButton(SuplaDeviceClass *sdc);
  bool disableActionsInConfigMode() override;
  void dontUseOnLoadConfig();

  uint8_t getMaxMulticlickValue();
  int8_t getButtonNumber() const override;
  void setButtonNumber(int8_t number);

 protected:
  void evaluateMaxMulticlickValue();
  // disbles repeating "on hold" if repeat time is lower than threshold
  // threshold 0 disables always
  void disableRepeatOnHold(uint32_t threshold = 0);
  void enableRepeatOnHold();
  unsigned int holdTimeMs = 0;
  unsigned int repeatOnHoldMs = 0;
  bool repeatOnHoldEnabled = false;
  unsigned int multiclickTimeMs = 0;
  uint32_t lastStateChangeMs = 0;
  uint8_t clickCounter = 0;
  uint8_t maxMulticlickValueConfigured = 0;
  unsigned int holdSend = 0;
  ButtonType buttonType = ButtonType::MONOSTABLE;
  bool configButton = false;
  bool useOnLoadConfig = true;
  int8_t buttonNumber = -1;

  static int buttonCounter;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BUTTON_H_
