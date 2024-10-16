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

#include "roller_shutter_parameters.h"

#include <supla/control/roller_shutter.h>
#include <supla/storage/storage.h>
#include <supla/network/web_sender.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/channels/channel.h>

#include <stdio.h>
#include <string.h>
#include "supla/storage/config.h"
#include "supla/storage/config_tags.h"

using Supla::Html::RollerShutterParameters;

RollerShutterParameters::RollerShutterParameters(
    Supla::Control::RollerShutter* rs)
    : HtmlElement(HTML_SECTION_FORM), rs(rs) {
}

RollerShutterParameters::~RollerShutterParameters() {
}

void RollerShutterParameters::setRsPtr(Supla::Control::RollerShutter *rs) {
  this->rs = rs;
}

void RollerShutterParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (rs == nullptr || rs->getChannel() == nullptr || cfg == nullptr) {
    return;
  }

  char key[16] = {};
  int32_t channelFunc = rs->getChannel()->getDefaultFunction();

  char tmp[100] = {};
  snprintf(tmp,
           sizeof(tmp),
           "%s #%d",
           Supla::getRelayChannelName(channelFunc),
           rs->getChannelNumber());

  sender->send("</div><div class=\"box\">");
  sender->send("<h3>");
  sender->send(tmp);
  sender->send("</h3>");

  // form-field BEGIN
  rs->generateKey(key, Supla::ConfigTag::ChannelFunctionTag);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Channel function");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
//  switch (channelFunction) {
//    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
//    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
//    case SUPLA_CHANNELFNC_TERRACE_AWNING: {
//    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR: {
//    case SUPLA_CHANNELFNC_CURTAIN: {
//    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN: {
//    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND: {
//    case SUPLA_CHANNELFNC_VERTICAL_BLIND: {
  if (rs->isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER,
        Supla::getRelayChannelName(
            SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER),
        channelFunc == SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  }
  if (rs->isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW,
        Supla::getRelayChannelName(
            SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW),
        channelFunc == SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW);
  }
  if (rs->isFunctionSupported(SUPLA_CHANNELFNC_TERRACE_AWNING)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_TERRACE_AWNING,
        Supla::getRelayChannelName(SUPLA_CHANNELFNC_TERRACE_AWNING),
        channelFunc == SUPLA_CHANNELFNC_TERRACE_AWNING);
  }
  if (rs->isFunctionSupported(SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR,
        Supla::getRelayChannelName(SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR),
        channelFunc == SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR);
  }
  if (rs->isFunctionSupported(SUPLA_CHANNELFNC_CURTAIN)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_CURTAIN,
        Supla::getRelayChannelName(SUPLA_CHANNELFNC_CURTAIN),
        channelFunc == SUPLA_CHANNELFNC_CURTAIN);
  }
  if (rs->isFunctionSupported(SUPLA_CHANNELFNC_PROJECTOR_SCREEN)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_PROJECTOR_SCREEN,
        Supla::getRelayChannelName(SUPLA_CHANNELFNC_PROJECTOR_SCREEN),
        channelFunc == SUPLA_CHANNELFNC_PROJECTOR_SCREEN);
  }
  if (rs->isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND,
        Supla::getRelayChannelName(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND),
        channelFunc == SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  }
  if (rs->isFunctionSupported(SUPLA_CHANNELFNC_VERTICAL_BLIND)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_VERTICAL_BLIND,
        Supla::getRelayChannelName(SUPLA_CHANNELFNC_VERTICAL_BLIND),
        channelFunc == SUPLA_CHANNELFNC_VERTICAL_BLIND);
  }
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  if (rs->getMotorUpsideDown() != 0) {
    // form-field BEGIN
    rs->generateKey(key, Supla::ConfigTag::RollerShutterMotorUpsideDownTag);
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(key, "Motor upside down");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(key);
    sender->send(">");
    sender->sendSelectItem(0, "NO", rs->getMotorUpsideDown() != 2);
    sender->sendSelectItem(1, "YES", rs->getMotorUpsideDown() == 2);
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }

  if (rs->getButtonsUpsideDown() != 0) {
    // form-field BEGIN
    rs->generateKey(key, Supla::ConfigTag::RollerShutterButtonsUpsideDownTag);
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(key, "Buttons upside down");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(key);
    sender->send(">");
    sender->sendSelectItem(0, "NO", rs->getButtonsUpsideDown() != 2);
    sender->sendSelectItem(1, "YES", rs->getButtonsUpsideDown() == 2);
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }

  // form-field BEGIN
  rs->generateKey(key, Supla::ConfigTag::RollerShutterTimeMarginTag);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Time margin (%)");
  sender->send("<div>");
  sender->send("<input ");
  sender->sendNameAndId(key);
  sender->send(
      " type=\"number\" min=\"-1\" max=\"100\" step=\"1\" placeholder=\"Use -1 "
      "for default\" ");
  sender->send(" value=\"");
  sender->send(static_cast<int>(rs->getTimeMargin()));
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END;

  // form-field BEGIN
  rs->generateKey(key, Supla::ConfigTag::RollerShutterOpeningTimeTag);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Full opening time (sec.)");
  sender->send("<div>");
  sender->send("<input ");
  sender->sendNameAndId(key);
  sender->send(
      " type=\"number\" min=\"0\" max=\"300\" step=\"0.1\" ");
  if (rs->isAutoCalibrationSupported()) {
    sender->send(" placeholder=\"Use 0 for autocalibration\" ");
  }
  sender->send(" value=\"");
  sender->send(rs->getOpeningTimeMs() / 100, 1);
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END;

  // form-field BEGIN
  rs->generateKey(key, Supla::ConfigTag::RollerShutterClosingTimeTag);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Full closing time (sec.)");
  sender->send("<div>");
  sender->send("<input ");
  sender->sendNameAndId(key);
  sender->send(
      " type=\"number\" min=\"0\" max=\"300\" step=\"0.1\" ");
  if (rs->isAutoCalibrationSupported()) {
    sender->send(" placeholder=\"Use 0 for autocalibration\" ");
  }
  sender->send(" value=\"");
  sender->send(rs->getClosingTimeMs() / 100, 1);
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END;
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

  return false;
}
