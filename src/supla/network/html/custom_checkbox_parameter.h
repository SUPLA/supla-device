// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_CUSTOM_CHECKBOX_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_CUSTOM_CHECKBOX_PARAMETER_H_

#include <supla/network/html_element.h>
#include <stdint.h>

namespace Supla {

namespace Html {

/* This HTML Element provides input in config mode for integer value.
 * You have to provide paramTag under which provided value will be stored
 * in Supla::Storage::Config.
 * paramLabel provides label which is displayed next to input in www.
 */

#define MAX_LABEL_SIZE 500

class CustomCheckboxParameter : public HtmlElement {
 public:
  CustomCheckboxParameter(const char *paramTag,
                          const char *paramLabel,
                          uint8_t defaultValue = 0);
  virtual ~CustomCheckboxParameter();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

  void setTag(const char *tagValue);
  void setLabel(const char *labelValue);

 protected:
  char *tag = nullptr;
  char *label = nullptr;
  uint8_t checkboxValue;
  bool checkboxFound = false;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_CUSTOM_CHECKBOX_PARAMETER_H_
