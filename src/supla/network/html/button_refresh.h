// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_REFRESH_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_REFRESH_H_

#include <supla/network/html_element.h>

namespace Supla {

namespace Html {

class ButtonRefresh : public HtmlElement {
 public:
  ButtonRefresh();
  void send(Supla::WebSender* sender) override;
};

};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_BUTTON_REFRESH_H_
