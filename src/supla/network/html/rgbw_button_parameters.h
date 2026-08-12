// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_RGBW_BUTTON_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_RGBW_BUTTON_PARAMETERS_H_

#include <supla/network/html/select_input_parameter.h>

namespace Supla {

namespace Html {

class RgbwButtonParameters : public SelectInputParameter {
 public:
  // if id is -1, it is applied to all rgbw elements
  // otherwise it should be channel number
  explicit RgbwButtonParameters(int id = -1, const char *label = nullptr);

 protected:
};

};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_RGBW_BUTTON_PARAMETERS_H_
