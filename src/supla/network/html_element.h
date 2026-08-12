// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_ELEMENT_H_
#define SRC_SUPLA_NETWORK_HTML_ELEMENT_H_

namespace Supla {

class WebSender;

enum HtmlSection {
  HTML_SECTION_FORM,
  HTML_SECTION_DEVICE_INFO,
  HTML_SECTION_NETWORK,
  HTML_SECTION_PROTOCOL,
  HTML_SECTION_BETA_FORM,
  HTML_SECTION_BUTTON_BEFORE,
  HTML_SECTION_BUTTON_AFTER,
  HTML_SECTION_LOGS
};

class HtmlElement {
 public:
  static HtmlElement *begin();
  static HtmlElement *last();

  static const char *selected(bool isSelected);
  static const char *checked(bool isChecked);

  HtmlElement *next();

  explicit HtmlElement(HtmlSection section = HTML_SECTION_FORM);
  virtual ~HtmlElement();
  virtual void send(Supla::WebSender *sender) = 0;
  virtual bool handleResponse(const char *key, const char *value);
  virtual void onProcessingEnd();
  HtmlSection section;

 protected:
  static HtmlElement *firstPtr;
  HtmlElement *nextPtr = nullptr;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_ELEMENT_H_
