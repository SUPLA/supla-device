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

#ifndef SRC_SUPLA_CONTROL_GROUP_BUTTON_CONTROL_RGBW_H_
#define SRC_SUPLA_CONTROL_GROUP_BUTTON_CONTROL_RGBW_H_

#include <supla/action_handler.h>
#include <supla/element.h>
#include <supla/control/rgbw_base.h>

#define SUPLA_MAX_GROUP_CONTROL_ELEMENTS 10

namespace Supla::Control {

class Button;
class RGBWBase;

class GroupButtonControlRgbw : public ActionHandler, public Element {
 public:
  explicit GroupButtonControlRgbw(Button *button = nullptr);
  void attach(Button *button);
  void addToGroup(RGBWBase *rgbwElement);

  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void onInit() override;
  void handleAction(int event, int action) override;

  void setButtonControlType(int rgbwChannelNumber,
                            RGBWBase::ButtonControlType type);

 private:
  void handleTurnOn();
  void handleTurnOff();
  void handleToggle();
  void handleIterate();

  Button *attachedButton = nullptr;
  RGBWBase *rgbw[SUPLA_MAX_GROUP_CONTROL_ELEMENTS] = {};
  RGBWBase::ButtonControlType controlType[SUPLA_MAX_GROUP_CONTROL_ELEMENTS] =
      {};
  int rgbwCount = 0;
};

}  // namespace Supla::Control

#endif  // SRC_SUPLA_CONTROL_GROUP_BUTTON_CONTROL_RGBW_H_
