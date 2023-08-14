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

#include "disable_user_interface_parameter.h"

#include <SuplaDevice.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>

using Supla::Html::DisableUserInterfaceParameter;

DisableUserInterfaceParameter::DisableUserInterfaceParameter()
    : HtmlElement(HTML_SECTION_FORM) {
}

DisableUserInterfaceParameter::~DisableUserInterfaceParameter() {
}

void DisableUserInterfaceParameter::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint8_t value = 0;  // default value
    cfg->getUInt8(Supla::Html::DisableUserInterfaceCfgTag, &value);

    // form-field BEGIN
    sender->send("<div class=\"form-field right-checkbox\">");
    sender->sendLabelFor(Supla::Html::DisableUserInterfaceCfgTag,
                         "Disable local user interface");
    sender->send("<label>");
    sender->send("<div class=\"switch\">");
    sender->send("<input type=\"checkbox\" value=\"on\" ");
    sender->send(checked(value == 1));
    sender->sendNameAndId(Supla::Html::DisableUserInterfaceCfgTag);
    sender->send(">");
    sender->send("<span class=\"slider\"></span>");
    sender->send("</div>");
    sender->send("</label>");
    sender->send("</div>");
    // form-field BEGIN
  }
}

bool DisableUserInterfaceParameter::handleResponse(const char* key,
                                                   const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && strcmp(key, Supla::Html::DisableUserInterfaceCfgTag) == 0) {
    checkboxFound = true;
    uint8_t currentValue = 0;  // default value
    cfg->getUInt8(Supla::Html::DisableUserInterfaceCfgTag, &currentValue);

    uint8_t disableUI = (strcmp(value, "on") == 0 ? 1 : 0);

    if (disableUI != currentValue) {
      cfg->setUInt8(Supla::Html::DisableUserInterfaceCfgTag, disableUI);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_DISABLE_USER_INTERFACE);
    }
    return true;
  }
  return false;
}

void DisableUserInterfaceParameter::onProcessingEnd() {
  if (!checkboxFound) {
    // checkbox doesn't send value when it is not checked, so on processing end
    // we check if it was found earlier, and if not, then we process it as "off"
    handleResponse(Supla::Html::DisableUserInterfaceCfgTag, "off");
  }
  checkboxFound = false;
}

