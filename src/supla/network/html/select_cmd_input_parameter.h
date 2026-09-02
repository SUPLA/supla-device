// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_SELECT_CMD_INPUT_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_SELECT_CMD_INPUT_PARAMETER_H_

#include <supla/network/html/text_cmd_input_parameter.h>

namespace Supla {

namespace Html {

class SelectCmdInputParameter : public TextCmdInputParameter {
 public:
  SelectCmdInputParameter(const char *paramTag, const char *paramLabel);
  virtual ~SelectCmdInputParameter();
  void send(Supla::WebSender* sender) override;
};

};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_SELECT_CMD_INPUT_PARAMETER_H_
