// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_HOME_SCREEN_CONTENT_H_
#define SRC_SUPLA_NETWORK_HTML_HOME_SCREEN_CONTENT_H_

#include <supla/network/html/select_input_parameter.h>

namespace Supla {

namespace Html {
class HomeScreenContentParameters : public SelectInputParameter {
 public:
  explicit HomeScreenContentParameters(const char *label);
  void onProcessingEnd() override;
  void initFields(uint64_t fieldBits);

 private:
  bool channelConfigChanged = false;
};

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_HOME_SCREEN_CONTENT_H_
