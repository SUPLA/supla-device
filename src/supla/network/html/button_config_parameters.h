// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_CONFIG_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_CONFIG_PARAMETERS_H_

#include <supla/network/html/select_input_parameter.h>

namespace Supla {

namespace Html {

class ButtonConfigParameters : public SelectInputParameter {
 public:
  explicit ButtonConfigParameters(int id = -1);
};

};  // namespace Html
};  // namespace Supla
#endif  // SRC_SUPLA_NETWORK_HTML_BUTTON_CONFIG_PARAMETERS_H_
