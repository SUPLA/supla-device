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

#ifndef ARDUINO_ARCH_AVR
#include "roller_shutter_parameters.h"

#include <stdio.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/control/roller_shutter.h>
#include <supla/log_wrapper.h>
#include <supla/network/web_sender.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

#include "supla/storage/config.h"
#include "supla/storage/config_tags.h"

using Supla::Html::RollerShutterParameters;

RollerShutterParameters::RollerShutterParameters(
    Supla::Control::RollerShutter* rs)
    : HtmlElement(HTML_SECTION_FORM), rs(rs) {
}

RollerShutterParameters::~RollerShutterParameters() {
}

void RollerShutterParameters::setRsPtr(Supla::Control::RollerShutter* rs) {
  this->rs = rs;
}

void RollerShutterParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (rs == nullptr || rs->getChannel() == nullptr || cfg == nullptr) {
    return;
  }
  auto emitField = [&](const char* keyName, const char* label, auto&& render) {
    sender->formField([&]() {
      sender->labelFor(keyName, label);
      sender->tag("div").body([&]() { render(); });
    });
  };

  auto emitSelectField =
      [&](const char* keyName, const char* label, auto&& renderOptions) {
        emitField(keyName, label, [&]() {
          auto select = sender->selectTag(keyName, keyName);
          select.body([&]() { renderOptions(); });
        });
      };

  auto emitYesNoField =
      [&](const char* keyName, const char* label, bool yesSelected) {
        emitSelectField(keyName, label, [&]() {
          sender->selectOption(0, "NO", !yesSelected);
          sender->selectOption(1, "YES", yesSelected);
        });
      };

  char key[16] = {};
  int32_t channelFunc = rs->getChannel()->getDefaultFunction();

  char tmp[100] = {};
  snprintf(tmp,
           sizeof(tmp),
           "%s #%d",
           Supla::getRelayChannelName(channelFunc),
           rs->getChannelNumber());

  sender->send("</div><div class=\"box\">");
  sender->tag("h3").body(tmp);

  rs->generateKey(key, Supla::ConfigTag::ChannelFunctionTag);
  emitSelectField(key, "Channel function", [&]() {
    if (rs->isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER,
          Supla::getRelayChannelName(
              SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER),
          channelFunc == SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
    }
    if (rs->isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW,
          Supla::getRelayChannelName(SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW),
          channelFunc == SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW);
    }
    if (rs->isFunctionSupported(SUPLA_CHANNELFNC_TERRACE_AWNING)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_TERRACE_AWNING,
          Supla::getRelayChannelName(SUPLA_CHANNELFNC_TERRACE_AWNING),
          channelFunc == SUPLA_CHANNELFNC_TERRACE_AWNING);
    }
    if (rs->isFunctionSupported(SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR,
          Supla::getRelayChannelName(SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR),
          channelFunc == SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR);
    }
    if (rs->isFunctionSupported(SUPLA_CHANNELFNC_CURTAIN)) {
      sender->selectOption(SUPLA_CHANNELFNC_CURTAIN,
                           Supla::getRelayChannelName(SUPLA_CHANNELFNC_CURTAIN),
                           channelFunc == SUPLA_CHANNELFNC_CURTAIN);
    }
    if (rs->isFunctionSupported(SUPLA_CHANNELFNC_PROJECTOR_SCREEN)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_PROJECTOR_SCREEN,
          Supla::getRelayChannelName(SUPLA_CHANNELFNC_PROJECTOR_SCREEN),
          channelFunc == SUPLA_CHANNELFNC_PROJECTOR_SCREEN);
    }
    if (rs->isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND,
          Supla::getRelayChannelName(
              SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND),
          channelFunc == SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
    }
    if (rs->isFunctionSupported(SUPLA_CHANNELFNC_VERTICAL_BLIND)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_VERTICAL_BLIND,
          Supla::getRelayChannelName(SUPLA_CHANNELFNC_VERTICAL_BLIND),
          channelFunc == SUPLA_CHANNELFNC_VERTICAL_BLIND);
    }
  });

  if (rs->getMotorUpsideDown() != 0) {
    rs->generateKey(key, Supla::ConfigTag::RollerShutterMotorUpsideDownTag);
    emitYesNoField(key, "Motor upside down", rs->getMotorUpsideDown() == 2);
  }

  if (rs->getButtonsUpsideDown() != 0) {
    rs->generateKey(key, Supla::ConfigTag::RollerShutterButtonsUpsideDownTag);
    emitYesNoField(key, "Buttons upside down", rs->getButtonsUpsideDown() == 2);
  }

  rs->generateKey(key, Supla::ConfigTag::RollerShutterTimeMarginTag);
  emitField(key, "Time margin (%)", [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "number")
        .attr("min", -1)
        .attr("max", 100)
        .attr("step", 1)
        .attr("placeholder", "Use -1 for default")
        .attr("name", key)
        .attr("id", key)
        .attr("value", static_cast<int>(rs->getTimeMargin()))
        .finish();
  });

  rs->generateKey(key, Supla::ConfigTag::RollerShutterOpeningTimeTag);
  emitField(key, "Full opening time (sec.)", [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "number")
        .attr("min", 0)
        .attr("max", 300)
        .attr("step", 1, 1)
        .attr("name", key)
        .attr("id", key);
    if (rs->isAutoCalibrationSupported()) {
      input.attr("placeholder", "Use 0 for autocalibration");
    }
    input.attr("value", rs->getOpeningTimeMs() / 100, 1).finish();
  });

  rs->generateKey(key, Supla::ConfigTag::RollerShutterClosingTimeTag);
  emitField(key, "Full closing time (sec.)", [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "number")
        .attr("min", 0)
        .attr("max", 300)
        .attr("step", 1, 1)
        .attr("name", key)
        .attr("id", key);
    if (rs->isAutoCalibrationSupported()) {
      input.attr("placeholder", "Use 0 for autocalibration");
    }
    input.attr("value", rs->getClosingTimeMs() / 100, 1).finish();
  });

  if (rs->isTiltFunctionEnabled()) {
    rs->generateKey(key, Supla::ConfigTag::FacadeBlindTiltControlTypeTag);
    emitSelectField(key, "Tilt control type", [&]() {
      sender->selectOption(
          0,
          "OFF",
          rs->getTiltControlType() == SUPLA_TILT_CONTROL_TYPE_UNKNOWN);
      sender->selectOption(
          1,
          "Stands in position while tilting",
          rs->getTiltControlType() ==
              SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING);
      sender->selectOption(
          2,
          "Changes position while tilting",
          rs->getTiltControlType() ==
              SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING);
      sender->selectOption(
          3,
          "Tils only when fully closed",
          rs->getTiltControlType() ==
              SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED);
    });

    rs->generateKey(key, Supla::ConfigTag::FacadeBlindTiltingTimeTag);
    emitField(key, "Tilting time (sec.)", [&]() {
      auto input = sender->voidTag("input");
      input.attr("type", "number")
          .attr("min", 0)
          .attr("max", 300)
          .attr("step", 1, 1)
          .attr("placeholder", "Use 0 for default")
          .attr("name", key)
          .attr("id", key)
          .attr("value", static_cast<int>(rs->getTiltingTimeMs() / 100), 1)
          .finish();
    });
  }
}

bool RollerShutterParameters::handleResponse(const char* key,
                                             const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (rs == nullptr || rs->getChannel() == nullptr || cfg == nullptr) {
    return false;
  }

  char keyMatch[16] = {};
  rs->generateKey(keyMatch, Supla::ConfigTag::ChannelFunctionTag);

  // channel function
  if (strcmp(key, keyMatch) == 0) {
    int32_t channelFunc = stringToUInt(value);
    if (rs->isFunctionSupported(channelFunc)) {
      rs->setAndSaveFunction(channelFunc);
    } else {
      SUPLA_LOG_WARNING("RsHtml: Unsupported channel function: %d",
                        channelFunc);
      return true;
    }
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::RollerShutterMotorUpsideDownTag);
  if (strcmp(key, keyMatch) == 0) {
    int32_t enabled = stringToUInt(value);
    rs->setRsConfigMotorUpsideDownValue(enabled == 1 ? 2 : 1);
    return true;
  }

  rs->generateKey(keyMatch,
                  Supla::ConfigTag::RollerShutterButtonsUpsideDownTag);
  if (strcmp(key, keyMatch) == 0) {
    int32_t enabled = stringToUInt(value);
    rs->setRsConfigButtonsUpsideDownValue(enabled == 1 ? 2 : 1);
    return true;
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::RollerShutterTimeMarginTag);
  if (strcmp(key, keyMatch) == 0) {
    int32_t timeMargin = stringToUInt(value);
    rs->setRsConfigTimeMarginValue(timeMargin);
    return true;
  }

  // open close time
  rs->generateKey(keyMatch, Supla::ConfigTag::RollerShutterOpeningTimeTag);
  if (strcmp(key, keyMatch) == 0) {
    uint32_t time = floatStringToInt(value, 1) * 100;
    auto closingTime = rs->getClosingTimeMs();
    rs->setOpenCloseTime(closingTime, time);
    return true;
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::RollerShutterClosingTimeTag);
  if (strcmp(key, keyMatch) == 0) {
    uint32_t time = floatStringToInt(value, 1) * 100;
    auto openingTime = rs->getOpeningTimeMs();
    rs->setOpenCloseTime(time, openingTime);
    return true;
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::FacadeBlindTiltingTimeTag);
  if (strcmp(key, keyMatch) == 0) {
    uint32_t time = floatStringToInt(value, 1) * 100;
    rs->setTiltingTime(time);
    return true;
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::FacadeBlindTiltControlTypeTag);
  if (strcmp(key, keyMatch) == 0) {
    uint32_t tiltControlType = stringToUInt(value);
    rs->setTiltControlType(tiltControlType);
    return true;
  }

  return false;
}

#endif  // ARDUINO_ARCH_AVR
