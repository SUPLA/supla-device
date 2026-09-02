// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_SECURITY_LOG_LIST_H_
#define SRC_SUPLA_NETWORK_HTML_SECURITY_LOG_LIST_H_

#include <supla/network/html_element.h>

namespace Supla {

namespace Device {
class SecurityLogger;
};  // namespace Device

namespace Html {

class SecurityLogList : public HtmlElement {
 public:
  explicit SecurityLogList(Supla::Device::SecurityLogger *logger);
  virtual ~SecurityLogList();
  void send(Supla::WebSender* sender) override;

 private:
  Supla::Device::SecurityLogger *logger = nullptr;
};

};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_SECURITY_LOG_LIST_H_
