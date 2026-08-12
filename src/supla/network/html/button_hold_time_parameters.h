// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_HOLD_TIME_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_HOLD_TIME_PARAMETERS_H_

#include <supla/network/html_element.h>
#include <stdint.h>

namespace Supla {

namespace Html {

class ButtonHoldTimeParameters : public HtmlElement {
 public:
  explicit ButtonHoldTimeParameters(uint32_t defaultHoldTime = 700);
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

 protected:
  uint32_t defaultHoldTime = 700;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_BUTTON_HOLD_TIME_PARAMETERS_H_
