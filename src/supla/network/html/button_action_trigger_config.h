// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_ACTION_TRIGGER_CONFIG_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_ACTION_TRIGGER_CONFIG_H_

#include <supla/network/html_element.h>

namespace Supla {
namespace Html {

class ButtonActionTriggerConfig : public HtmlElement {
 public:
  explicit ButtonActionTriggerConfig(int channelNumber,
                                     int buttonNumber,
                                     const char* labelPrefix = nullptr);
  virtual ~ButtonActionTriggerConfig();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

 protected:
  int channelNumber = 0;
  int buttonNumber = 0;
  char *labelPrefix = nullptr;
};

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_BUTTON_ACTION_TRIGGER_CONFIG_H_
