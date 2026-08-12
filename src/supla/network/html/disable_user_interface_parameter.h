// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_DISABLE_USER_INTERFACE_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_DISABLE_USER_INTERFACE_PARAMETER_H_

#include <supla/network/html_element.h>

namespace Supla {

namespace Html {

class DisableUserInterfaceParameter : public HtmlElement {
 public:
  DisableUserInterfaceParameter();
  virtual ~DisableUserInterfaceParameter();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

 private:
  bool change = false;
};

};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_DISABLE_USER_INTERFACE_PARAMETER_H_
