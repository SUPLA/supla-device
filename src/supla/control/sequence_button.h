// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_SEQUENCE_BUTTON_H_
#define SRC_SUPLA_CONTROL_SEQUENCE_BUTTON_H_

#include <stdint.h>

#include "simple_button.h"

namespace Supla {
namespace Io {
class Base;
}  // namespace Io

namespace Control {

#define SEQUENCE_MAX_SIZE 30

struct ClickSequence {
  uint16_t data[SEQUENCE_MAX_SIZE];
};

class SequenceButton : public SimpleButton {
 public:
  explicit SequenceButton(Supla::Io::IoPin inputPin);
  explicit SequenceButton(Supla::Io::Base *io,
                          int pin,
                          bool pullUp = false,
                          bool invertLogic = false);
  explicit SequenceButton(int pin,
                          bool pullUp = false,
                          bool invertLogic = false);

  void onTimer();

  void setSequence(uint16_t *sequence);
  void setMargin(float);
  void getLastRecordedSequence(uint16_t *sequence) const;

 protected:
  unsigned int calculateMargin(unsigned int);

  uint32_t lastStateChangeMs = 0;
  uint16_t longestSequenceTimeDeltaWithMargin = 800;
  uint8_t clickCounter = 0;
  bool sequenceDetectecion = true;
  float margin = 0.3;

  ClickSequence currentSequence = {};
  ClickSequence matchSequence = {};
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_SEQUENCE_BUTTON_H_
