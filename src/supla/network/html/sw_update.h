// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_SW_UPDATE_H_
#define SRC_SUPLA_NETWORK_HTML_SW_UPDATE_H_

#include <supla/network/html_element.h>

class SuplaDeviceClass;

namespace Supla {

namespace Html {

class SwUpdate : public HtmlElement {
 public:
  explicit SwUpdate(SuplaDeviceClass* sdc);
  virtual ~SwUpdate();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

 protected:
  SuplaDeviceClass* sdc = nullptr;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_SW_UPDATE_H_
