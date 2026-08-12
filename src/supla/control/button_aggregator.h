// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_BUTTON_AGGREGATOR_H_
#define SRC_SUPLA_CONTROL_BUTTON_AGGREGATOR_H_

#include <supla/control/button.h>
#include <stdint.h>

#define BUTTON_AGGREGATOR_MAX_BUTTONS 10

namespace Supla {
namespace Control {

class ButtonAggregator : public Supla::Control::Button {
 public:
  ButtonAggregator();
  virtual ~ButtonAggregator();

  void onTimer() override;
  void handleAction(int event, int action) override;
  bool addButton(Supla::Control::Button* button);

 protected:
  Supla::Control::Button* buttons[BUTTON_AGGREGATOR_MAX_BUTTONS] = {};
  int pressCount = 0;
  int buttonCount = 0;
  bool stateChanged = false;
  uint32_t allPressedTimestamp = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BUTTON_AGGREGATOR_H_
