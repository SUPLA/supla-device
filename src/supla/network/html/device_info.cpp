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

#include <SuplaDevice.h>
#include <supla/channel.h>
#include <supla/network/html/device_info.h>
#include <supla/network/network.h>
#include <supla/network/web_sender.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <string.h>
#include <supla/device/register_device.h>

namespace Supla {

namespace Html {

DeviceInfo::DeviceInfo(SuplaDeviceClass *sdc)
    : HtmlElement(HTML_SECTION_DEVICE_INFO), sdc(sdc) {
}

DeviceInfo::~DeviceInfo() {}

void DeviceInfo::send(Supla::WebSender *sender) {
  sender->send("<h1>");
  sender->send(Supla::RegisterDevice::getName());
  sender->send("</h1><span>");
  if (sdc && sdc->prepareLastStateLog()) {
    sender->send("LAST STATE: ");

    bool firstElemenet = true;
    while (char *lastState = sdc->getLastStateLog()) {
      if (!firstElemenet) {
        sender->send(", ");
      }
      firstElemenet = false;

      sender->send(lastState);
    }
  }
  sender->send("<br>Firmware: ");
  sender->send(Supla::RegisterDevice::getSoftVer());
  sender->send("<br>GUID: ");
  char buf[512] = {};
  generateHexString(Supla::RegisterDevice::getGUID(), buf, SUPLA_GUID_SIZE);
  sender->send(buf);
  uint8_t mac[6] = {};
  if (Supla::Network::GetMainMacAddr(mac)) {
    sender->send("<br>MAC: ");
    generateHexString(mac, buf, 6, ':');
    sender->send(buf);
  }
  sender->send("</span><span>");
  if (Supla::Network::GetNetIntfCount() > 1) {
    for (auto net = Supla::Network::FirstInstance(); net;
         net = net->NextInstance(net)) {
      uint8_t curMac[6] = {};
      if (net->getMacAddr(curMac) && memcmp(curMac, mac, 6) != 0) {
        generateHexString(curMac, buf, 6, ':');
        sender->send("<br>");
        sender->send(net->getIntfName());
        sender->send(" MAC: ");
        sender->send(buf);
      }
    }
  }
  sender->send("<br>Uptime: ");
  sender->send(sdc->uptime.getUptime());
  sender->send(" s</span>");
}
};  // namespace Html
};  // namespace Supla
