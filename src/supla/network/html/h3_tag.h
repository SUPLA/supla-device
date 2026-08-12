// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_H3_TAG_H_
#define SRC_SUPLA_NETWORK_HTML_H3_TAG_H_

#include <supla/network/html_element.h>

namespace Supla {

namespace Html {

/**
 * @deprecated Use WebSender::tag("h3").body(text) instead.
 *
 * This class is a thin wrapper around a single heading tag and is kept only
 * for compatibility with older callers.
 */
class H3Tag : public HtmlElement {
 public:
  explicit H3Tag(const char *text);
  virtual ~H3Tag();
  void send(Supla::WebSender* sender) override;

 protected:
  char *text = nullptr;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_H3_TAG_H_
