// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_MULTICLICK_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_MULTICLICK_PARAMETERS_H_

#include <supla/network/html_element.h>

namespace Supla {

namespace Html {

class ButtonMulticlickParameters : public HtmlElement {
 public:
  ButtonMulticlickParameters();
  virtual ~ButtonMulticlickParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
};

};  // namespace Html
};  // namespace Supla



#endif  // SRC_SUPLA_NETWORK_HTML_BUTTON_MULTICLICK_PARAMETERS_H_
