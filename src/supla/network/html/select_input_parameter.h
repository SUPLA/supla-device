// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_SELECT_INPUT_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_SELECT_INPUT_PARAMETER_H_

#include <supla/network/html_element.h>
#include <supla/local_action.h>
#include <stdint.h>

namespace Supla {

namespace Html {

struct SelectValueMapElement {
  char *name = nullptr;
  int value = 0;
  SelectValueMapElement *next = nullptr;

  ~SelectValueMapElement();
};

class SelectInputParameter : public HtmlElement, public LocalAction {
 public:
  SelectInputParameter();
  SelectInputParameter(const char *paramTag, const char *paramLabel);
  virtual ~SelectInputParameter();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void registerValue(const char *name, int value);
  void setTag(const char *tagValue);
  void setLabel(const char *labelValue);
  void onProcessingEnd() override;

  void setBaseTypeBitCount(uint8_t value);

 protected:
  char *tag = nullptr;
  char *label = nullptr;
  SelectValueMapElement *firstValue = nullptr;
  bool configChanged = false;
  uint8_t baseTypeBitCount = 32;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_SELECT_INPUT_PARAMETER_H_
