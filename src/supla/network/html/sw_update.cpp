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
#include <supla/device/auto_update_policy.h>
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
    bool skipCert = cfg->isSwUpdateSkipCert();
    bool useRemoteOta = false;
    Supla::AutoUpdatePolicy otaPolicy = Supla::AutoUpdatePolicy::SecurityOnly;
    if (sdc) {
      if (sdc->isAutomaticFirmwareUpdateEnabled()) {
        useRemoteOta = true;
        otaPolicy = cfg->getAutoUpdatePolicy();
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
      sender->send(selected(otaPolicy == Supla::AutoUpdatePolicy::ForcedOff));
      sender->send(
          ">Disabled on a device (can't be changed remotely, updates possible "
          "only via local web interface)</option>"
          "<option value=\"1\"");
      sender->send(selected(otaPolicy == Supla::AutoUpdatePolicy::Disabled));
      sender->send(
          ">Allow only manual updates (triggered by user via Cloud or via "
          "local web interface)</option>"
          "<option value=\"2\"");
      sender->send(
          selected(otaPolicy == Supla::AutoUpdatePolicy::SecurityOnly));
      sender->send(
          ">Install only security updates automatically</option>"
          "<option value=\"3\"");
      sender->send(selected(otaPolicy == Supla::AutoUpdatePolicy::AllUpdates));
      sender->send(">Install all updates automatically</option></select>");
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
    sender->send(selected(update && !skipCert));
    sender->send(
        ">YES</option>"

        "<option value=\"2\"");
    sender->send(selected(update && skipCert));
    sender->send(
        ">YES - SKIP CERTIFICATE (dangerous)</option></select>");
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
        cfg->setSwUpdateSkipCert(false);
        cfg->setSwUpdateBeta(false);
        break;
      }
      case 1: {
        cfg->setDeviceMode(DEVICE_MODE_SW_UPDATE);
        cfg->setSwUpdateSkipCert(false);
        cfg->setSwUpdateBeta(false);
        break;
      }
      case 2: {
        cfg->setDeviceMode(DEVICE_MODE_SW_UPDATE);
        cfg->setSwUpdateSkipCert(true);
        cfg->setSwUpdateBeta(false);
        break;
      }
    }
    return true;
  }
  if (strcmp(key, "otamode") == 0) {
    int otaMode = stringToUInt(value);
    if (otaMode < 0 || otaMode > SUPLA_FIRMWARE_UPDATE_POLICY_ALL_ENABLED) {
      return true;
    }
    Supla::AutoUpdatePolicy policy =
        static_cast<Supla::AutoUpdatePolicy>(otaMode);
    cfg->setAutoUpdatePolicy(policy);
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla
