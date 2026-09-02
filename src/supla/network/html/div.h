// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_DIV_H_
#define SRC_SUPLA_NETWORK_HTML_DIV_H_

#include <supla/network/html_element.h>

class SuplaDeviceClass;

namespace Supla {

namespace Html {

/**
 * @deprecated Use WebSender::tag("div") / WebSender::voidTag() instead.
 *
 * This helper only wraps plain `<div>` emission. The new streaming builder in
 * WebSender provides the same functionality with clearer ownership and less
 * manual tag handling.
 */
class DivBegin : public HtmlElement {
 public:
  explicit DivBegin(const char *className = nullptr,
      const char *idName = nullptr);
  ~DivBegin();
  void send(Supla::WebSender* sender) override;

 protected:
  char *className = nullptr;
  char *idName = nullptr;
};

/**
 * @deprecated Use WebSender::tag("div") with body() instead.
 *
 * This is a legacy closing tag helper kept for compatibility with older
 * code paths.
 */
class DivEnd : public HtmlElement {
 public:
  void send(Supla::WebSender* sender) override;
};


};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_DIV_H_
