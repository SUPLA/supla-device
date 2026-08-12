// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_HIDE_SHOW_CONTAINER_H_
#define SRC_SUPLA_NETWORK_HTML_HIDE_SHOW_CONTAINER_H_

#include <supla/network/html_element.h>

class SuplaDeviceClass;

namespace Supla {

namespace Html {

class HideShowContainerBegin : public HtmlElement {
 public:
  explicit HideShowContainerBegin(const char *name);
  virtual ~HideShowContainerBegin();
  void send(Supla::WebSender* sender) override;

 protected:
  char *name = nullptr;
};

class HideShowContainerEnd : public HtmlElement {
 public:
  void send(Supla::WebSender* sender) override;
};


};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_HIDE_SHOW_CONTAINER_H_
