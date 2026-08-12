// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_TYPE_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_TYPE_PARAMETERS_H_

#include <supla/network/html/select_input_parameter.h>

namespace Supla {

namespace Html {

class ButtonTypeParameters : public SelectInputParameter {
 public:
  explicit ButtonTypeParameters(int id, const char *labelPrefix = nullptr);

  void addMonostableOption();
  void addBistableOption();
  void addMotionSensorOption();
  void addCentralControlOption();

  // adds: monostable, bistable, and motion sensor
  void addDefaultOptions();
 private:
  char *labelPrefix = nullptr;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_BUTTON_TYPE_PARAMETERS_H_
