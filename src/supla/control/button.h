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
#include <supla/action_handler.h>
#include "action_trigger.h"
#include "simple_button.h"

class SuplaDeviceClass;

namespace Supla {
namespace Control {

class Button : public SimpleButton, public ActionHandler {
 public:
  friend class ActionTrigger;
  enum class ButtonType : uint8_t {
    MONOSTABLE,
    BISTABLE,
    MOTION_SENSOR
  };

  enum class OnLoadConfigType : uint8_t {
    LOAD_FULL_CONFIG,
    LOAD_BUTTON_SETUP_ONLY,
    DONT_LOAD_CONFIG
  };

  explicit Button(Supla::Io *io,
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

 protected:
  void evaluateMaxMulticlickValue();
  // disbles repeating "on hold" if repeat time is lower than threshold
  // threshold 0 disables always
  void disableRepeatOnHold(uint32_t threshold = 0);
  void enableRepeatOnHold();
  uint32_t multiclickTimeMs = 0;
  uint32_t lastStateChangeMs = 0;
  uint16_t repeatOnHoldMs = 0;
  uint16_t holdSend = 0;
  uint16_t holdTimeMs = 0;
  ButtonType buttonType = ButtonType::MONOSTABLE;
  enum OnLoadConfigType onLoadConfigType = OnLoadConfigType::LOAD_FULL_CONFIG;

  uint8_t clickCounter = 0;
  uint8_t maxMulticlickValueConfigured = 0;
  bool repeatOnHoldEnabled = false;
  bool configButton = false;
  int8_t buttonNumber = -1;
  bool disabled = false;

  static int buttonCounter;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BUTTON_H_
