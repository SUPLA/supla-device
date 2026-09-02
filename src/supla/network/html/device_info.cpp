// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include <SuplaDevice.h>
#include <string.h>
#include <supla/channel.h>
#include <supla/device/register_device.h>
#include <supla/log_wrapper.h>
#include <supla/network/html/device_info.h>
#include <supla/network/network.h>
#include <supla/network/web_sender.h>
#include <supla/tools.h>

namespace Supla {

namespace Html {

DeviceInfo::DeviceInfo(SuplaDeviceClass *sdc)
    : HtmlElement(HTML_SECTION_DEVICE_INFO), sdc(sdc) {
}

DeviceInfo::~DeviceInfo() {
}

void DeviceInfo::send(Supla::WebSender *sender) {
  sender->tag("h1").body(Supla::RegisterDevice::getName());

  auto span = sender->tag("span");
  span.close();
  if (sdc && sdc->prepareLastStateLog()) {
    sender->send("LAST STATE: ");

    bool firstElemenet = true;
    while (char *lastState = sdc->getLastStateLog()) {
      if (!firstElemenet) {
        sender->send(", ");
      }
      firstElemenet = false;

      sender->sendSafe(lastState);
    }
  }

  sender->voidTag("br").finish();
  sender->send("Firmware: ");
  sender->send(Supla::RegisterDevice::getSoftVer());
  sender->voidTag("br").finish();
  sender->send("GUID: ");

  char buf[512] = {};
  generateHexString(Supla::RegisterDevice::getGUID(), buf, SUPLA_GUID_SIZE);
  sender->send(buf);
  uint8_t mac[6] = {};
  if (Supla::Network::GetMainMacAddr(mac)) {
    sender->voidTag("br").finish();
    sender->send("MAC: ");
    generateHexString(mac, buf, 6, ':');
    sender->send(buf);
  }

  span.end();

  auto span2 = sender->tag("span");
  span2.close();
  if (Supla::Network::GetNetIntfCount() > 1) {
    for (auto net = Supla::Network::FirstInstance(); net;
         net = net->NextInstance(net)) {
      uint8_t curMac[6] = {};
      if (net->getMacAddr(curMac) && memcmp(curMac, mac, 6) != 0) {
        generateHexString(curMac, buf, 6, ':');
        sender->voidTag("br").finish();
        sender->sendSafe(net->getIntfName());
        sender->send(" MAC: ");
        sender->send(buf);
      }
    }
  }
  sender->voidTag("br").finish();
  sender->send("Uptime: ");
  sender->send(sdc->uptime.getUptime());
  sender->send(" s");
  span2.end();
}
};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR
