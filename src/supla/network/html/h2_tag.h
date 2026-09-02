// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_H2_TAG_H_
#define SRC_SUPLA_NETWORK_HTML_H2_TAG_H_

#include <supla/network/html_element.h>

namespace Supla {

namespace Html {

/**
 * @deprecated Use WebSender::tag("h2").body(text) instead.
 *
 * This class is a thin wrapper around a single heading tag and is kept only
 * for compatibility with older callers.
 */
class H2Tag : public HtmlElement {
 public:
  explicit H2Tag(const char *text);
  virtual ~H2Tag();
  void send(Supla::WebSender* sender) override;

 protected:
  char *text = nullptr;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_H2_TAG_H_
