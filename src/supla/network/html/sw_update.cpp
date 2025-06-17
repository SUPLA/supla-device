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

#include "sw_update.h"

#include <string.h>
#include <supla/device/sw_update.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <SuplaDevice.h>
#include <supla/device/auto_update_mode.h>
#include <supla/storage/config_tags.h>

namespace Supla {

namespace Html {

SwUpdate::SwUpdate(SuplaDeviceClass* sdc)
    : HtmlElement(HTML_SECTION_FORM), sdc(sdc) {
}

SwUpdate::~SwUpdate() {
}

void SwUpdate::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    bool update = (cfg->getDeviceMode() == DEVICE_MODE_SW_UPDATE);
    bool useRemoteOta = false;
    Supla::AutoUpdateMode otaMode = Supla::AutoUpdateMode::SecurityOnly;
    if (sdc) {
      if (sdc->isAutomaticFirmwareUpdateEnabled()) {
        useRemoteOta = true;
        otaMode = sdc->getAutoUpdateMode();
      }
    }

    if (useRemoteOta) {
      // form-field BEGIN
      sender->send("<div class=\"form-field\">");
      const char keyOta[] = "otamode";
      sender->sendLabelFor(keyOta, "Automatic remote OTA updates");
      sender->send("<div>");
      sender->send("<select ");
      sender->sendNameAndId(keyOta);
      sender->send("><option value=\"0\"");
      sender->send(selected(otaMode == Supla::AutoUpdateMode::ForcedOff));
      sender->send(
          ">Forced off - allow only updates started manually from local "
          "web</option>"
          "<option value=\"1\"");
      sender->send(selected(otaMode == Supla::AutoUpdateMode::Disabled));
      sender->send(
          ">Disabled - no automatic updates</option>"
          "<option value=\"2\"");
      sender->send(selected(otaMode == Supla::AutoUpdateMode::SecurityOnly));
      sender->send(
          ">Security updates only</option>"
          "<option value=\"3\"");
      sender->send(selected(otaMode == Supla::AutoUpdateMode::AllUpdates));
      sender->send(">All updates</option></select>");
      sender->send("</div>");
      sender->send("</div>");
      // form-field END
    }

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    const char key[] = "upd";
    sender->sendLabelFor(key, "Firmware update");
    sender->send("<div>");
    sender->send(
        "<select ");
    sender->sendNameAndId(key);
    sender->send("><option value=\"0\"");
    sender->send(selected(!update));
    sender->send(
        ">NO</option>"
        "<option value=\"1\"");
    sender->send(selected(update));
    sender->send(
        ">YES</option></select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }
}

bool SwUpdate::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "upd") == 0) {
    int update = stringToUInt(value);
    switch (update) {
      default:
      case 0: {
        cfg->setDeviceMode(DEVICE_MODE_NORMAL);
        cfg->setSwUpdateBeta(false);
        break;
      }
      case 1: {
        cfg->setDeviceMode(DEVICE_MODE_SW_UPDATE);
        cfg->setSwUpdateBeta(false);
        break;
      }
    }
    return true;
  }
  if (strcmp(key, "otamode") == 0) {
    int otaMode = stringToUInt(value);
    switch (otaMode) {
      default:
      case SUPLA_FIRMWARE_UPDATE_MODE_SECURITY_ONLY: {
        cfg->setUInt8(Supla::ConfigTag::OtaModeTag,
                      SUPLA_FIRMWARE_UPDATE_MODE_SECURITY_ONLY);
        break;
      }
      case SUPLA_FIRMWARE_UPDATE_MODE_FORCED_OFF: {
        cfg->setUInt8(Supla::ConfigTag::OtaModeTag,
                      SUPLA_FIRMWARE_UPDATE_MODE_FORCED_OFF);
        break;
      }
        case SUPLA_FIRMWARE_UPDATE_MODE_DISABLED: {
        cfg->setUInt8(Supla::ConfigTag::OtaModeTag,
                      SUPLA_FIRMWARE_UPDATE_MODE_DISABLED);
        break;
      }
      case SUPLA_FIRMWARE_UPDATE_MODE_ALL_ENABLED: {
        cfg->setUInt8(Supla::ConfigTag::OtaModeTag,
                      SUPLA_FIRMWARE_UPDATE_MODE_ALL_ENABLED);
        break;
      }
    }
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla
