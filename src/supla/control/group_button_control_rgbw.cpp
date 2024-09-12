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

#include "group_button_control_rgbw.h"
#include <supla/control/button.h>
#include <supla/storage/config.h>
#include <supla/log_wrapper.h>
#include <supla/actions.h>
#include <supla/control/rgbw_base.h>
#include <supla/time.h>
#include <supla/control/rgb_base.h>
#include <supla/storage/config_tags.h>

using Supla::Control::GroupButtonControlRgbw;

GroupButtonControlRgbw::GroupButtonControlRgbw(Button *button)
    : attachedButton(button) {
}

void GroupButtonControlRgbw::attach(Button *button) {
  attachedButton = button;
}

void GroupButtonControlRgbw::addToGroup(RGBWBase *rgbwElement) {
  if (rgbwCount >= SUPLA_MAX_GROUP_CONTROL_ELEMENTS) {
    SUPLA_LOG_ERROR("GroupButtonControlRgbw: Too many RGBWs in group!");
    return;
  }
  if (rgbwElement == nullptr) {
    SUPLA_LOG_ERROR("GroupButtonControlRgbw: RGBW is null!");
    return;
  }
  rgbw[rgbwCount++] = rgbwElement;
}

void GroupButtonControlRgbw::handleTurnOn() {
  for (int i = 0; i < rgbwCount; i++) {
    switch (controlType[i]) {
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGB: {
        rgbw[i]->handleAction(0, Supla::TURN_ON_RGB);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGBW: {
        rgbw[i]->handleAction(0, Supla::TURN_ON);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_W: {
        rgbw[i]->handleAction(0, Supla::TURN_ON_W);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_NOT_USED: {
        break;
      }
    }
  }
}

void GroupButtonControlRgbw::handleTurnOff() {
  for (int i = 0; i < rgbwCount; i++) {
    switch (controlType[i]) {
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGB: {
        rgbw[i]->handleAction(0, Supla::TURN_OFF_RGB);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGBW: {
        rgbw[i]->handleAction(0, Supla::TURN_OFF);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_W: {
        rgbw[i]->handleAction(0, Supla::TURN_OFF_W);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_NOT_USED: {
        break;
      }
    }
  }
}

void GroupButtonControlRgbw::handleToggle() {
  bool isOn = false;
  // for toggle action, we have to check all channels from group:
  // if at least one is ON, then toggle will turn off
  for (int i = 0; i < rgbwCount; i++) {
    switch (controlType[i]) {
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGB: {
        if (rgbw[i]->isOnRGB()) {
          isOn = true;
        }
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGBW: {
        if (rgbw[i]->isOn()) {
          isOn = true;
        }
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_W: {
        if (rgbw[i]->isOnW()) {
          isOn = true;
        }
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_NOT_USED: {
        break;
      }
    }
  }

  if (isOn) {
    handleTurnOff();
  } else {
    handleTurnOn();
  }
}

void GroupButtonControlRgbw::handleIterate() {
  Supla::Control::RGBWBase *mainChannel = nullptr;
  int brightnessValue = -1;
  // first we look for first rgbw with enabled button, iterate brightness on it
  // and get new brightness value
  for (int i = 0; i < rgbwCount; i++) {
    switch (controlType[i]) {
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGB: {
        mainChannel = rgbw[i];
        mainChannel->handleAction(0, Supla::ITERATE_DIM_RGB);
        brightnessValue = mainChannel->getCurrentRGBBrightness();
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGBW: {
        mainChannel = rgbw[i];
        mainChannel->handleAction(0, Supla::ITERATE_DIM_ALL);
        // on iterate dim all, both W and RGB brighness are synced
        brightnessValue = mainChannel->getCurrentRGBBrightness();
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_W: {
        mainChannel = rgbw[i];
        mainChannel->handleAction(0, Supla::ITERATE_DIM_W);
        brightnessValue = mainChannel->getCurrentDimmerBrightness();
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_NOT_USED: {
        break;
      }
    }
    if (mainChannel != nullptr) {
      break;
    }
  }

  // then we apply new brightness to all other rgbws
  for (int i = 0; i < rgbwCount; i++) {
    if (mainChannel == rgbw[i]) {
      continue;
    }
    switch (controlType[i]) {
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGB: {
        rgbw[i]->setRGBW(-1, -1, -1, brightnessValue, -1);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_RGBW: {
        rgbw[i]->setRGBW(-1, -1, -1, brightnessValue, brightnessValue);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_FOR_W: {
        rgbw[i]->setRGBW(-1, -1, -1, -1, brightnessValue);
        break;
      }
      case Supla::Control::RGBWBase::ButtonControlType::BUTTON_NOT_USED: {
        break;
      }
    }
  }
}

void GroupButtonControlRgbw::handleAction(int event, int action) {
  (void)(event);
  (void)(action);
  switch (action) {
    case Supla::TURN_ON: {
      handleTurnOn();
      break;
    }
    case Supla::TURN_OFF: {
      handleTurnOff();
      break;
    }
    case Supla::TOGGLE: {
      handleToggle();
      break;
    }
    case Supla::ITERATE_DIM_ALL:
    case Supla::ITERATE_DIM_W:
    case Supla::ITERATE_DIM_RGB: {
      handleIterate();
      break;
    }
  }
}

void GroupButtonControlRgbw::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    for (int i = 0; i < rgbwCount; i++) {
      int channelNo = rgbw[i]->getChannelNumber();
      if (channelNo < 0) {
        continue;
      }

      Supla::Config::generateKey(
          key, channelNo, Supla::ConfigTag::RgbwButtonTag);
      int32_t rgbwButtonControlType = 0;
      if (cfg->getInt32(key, &rgbwButtonControlType)) {
        if (rgbwButtonControlType >= 0 && rgbwButtonControlType <= 4) {
          controlType[i] =
              static_cast<Supla::Control::RGBWBase::ButtonControlType>(
                  rgbwButtonControlType);
        }
      }
      SUPLA_LOG_DEBUG("RGBW group[%d] button control type: %d",
          channelNo, controlType[i]);
    }
  }
}

void GroupButtonControlRgbw::setButtonControlType(int rgbwIndex,
                            RGBWBase::ButtonControlType type) {
  if (rgbwIndex < 0 || rgbwIndex >= rgbwCount ||
      rgbwIndex >= SUPLA_MAX_GROUP_CONTROL_ELEMENTS) {
    return;
  }
  controlType[rgbwIndex] = type;
}

void GroupButtonControlRgbw::onInit() {
  if (attachedButton) {
    SUPLA_LOG_DEBUG("GroupButtonControl configuring attachedButton");
    if (attachedButton->isMonostable()) {
      SUPLA_LOG_DEBUG("GroupButtonControl configuring monostable button");
      attachedButton->addAction(
          Supla::ITERATE_DIM_ALL, this, Supla::ON_HOLD);
      attachedButton->addAction(Supla::TOGGLE, this, Supla::ON_CLICK_1);
    } else if (attachedButton->isBistable()) {
      SUPLA_LOG_DEBUG("GroupButtonControl configuring bistable button");
      attachedButton->addAction(
          Supla::TOGGLE, this, Supla::CONDITIONAL_ON_CHANGE);
    } else if (attachedButton->isMotionSensor()) {
      SUPLA_LOG_DEBUG("GroupButtonControl configuring motion sensor");
      attachedButton->addAction(Supla::TURN_ON, this, Supla::ON_PRESS);
      attachedButton->addAction(Supla::TURN_OFF, this, Supla::ON_RELEASE);

      if (attachedButton->getLastState() == Supla::Control::PRESSED) {
        handleTurnOn();
      } else {
        handleTurnOff();
      }
    } else {
      SUPLA_LOG_WARNING("GroupButtonControl: Unknown button type");
    }
  }
}

