// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_TEXT_CMD_INPUT_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_TEXT_CMD_INPUT_PARAMETER_H_

#include <supla/network/html_element.h>
#include <supla/local_action.h>
#include <stdint.h>

namespace Supla {

namespace Html {

struct RegisteredCmdActionMap {
  char *cmd = nullptr;
  int eventId = 0;
  RegisteredCmdActionMap *next = nullptr;

  ~RegisteredCmdActionMap();
};

class TextCmdInputParameter : public HtmlElement, public LocalAction {
 public:
  TextCmdInputParameter(const char *paramTag, const char *paramLabel);
  virtual ~TextCmdInputParameter();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void registerCmd(const char *cmdStr, int eventId);

 protected:
  char *tag = nullptr;
  char *label = nullptr;
  RegisteredCmdActionMap *firstCmd = nullptr;
};

};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_TEXT_CMD_INPUT_PARAMETER_H_
