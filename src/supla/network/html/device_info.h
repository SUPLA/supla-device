// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_DEVICE_INFO_H_
#define SRC_SUPLA_NETWORK_HTML_DEVICE_INFO_H_

#include <supla/network/html_element.h>

class SuplaDeviceClass;

namespace Supla {

namespace Html {

class DeviceInfo : public HtmlElement {
 public:
  explicit DeviceInfo(SuplaDeviceClass *sdc);
  virtual ~DeviceInfo();
  virtual void send(Supla::WebSender *sender);
  //    virtual bool handleResponse() = 0;
 protected:
  SuplaDeviceClass *sdc;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_DEVICE_INFO_H_
