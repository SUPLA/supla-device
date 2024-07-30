/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "sequence_button.h"

Supla::Control::SequenceButton::SequenceButton(Supla::Io *io,
                                               int pin,
                                               bool pullUp,
                                               bool invertLogic)
    : SimpleButton(io, pin, pullUp, invertLogic) {
}

Supla::Control::SequenceButton::SequenceButton(int pin,
                                               bool pullUp,
                                               bool invertLogic)
    : SimpleButton(pin, pullUp, invertLogic) {
}

void Supla::Control::SequenceButton::onTimer() {
  unsigned int timeDelta = millis() - lastStateChangeMs;
  bool stateChanged = false;
  enum Supla::Control::StateResults stateResult = state.update();
  if (stateResult == TO_PRESSED) {
    stateChanged = true;
    runAction(ON_PRESS);
    runAction(ON_CHANGE);
  } else if (stateResult == TO_RELEASED) {
    stateChanged = true;
    runAction(ON_RELEASE);
    runAction(ON_CHANGE);
  }

  if (stateChanged) {
    lastStateChangeMs = millis();
    if (clickCounter > 0 && clickCounter < SEQUENCE_MAX_SIZE + 1) {
      currentSequence.data[clickCounter - 1] = timeDelta;
    }
    if (clickCounter == 0) {
      memset(currentSequence.data, 0, sizeof(uint16_t[SEQUENCE_MAX_SIZE]));
    }
    clickCounter++;
  }

  if (!stateChanged) {
    if (clickCounter > 0 && stateResult == RELEASED) {
      if (timeDelta > longestSequenceTimeDeltaWithMargin) {
        SUPLA_LOG_DEBUG("Recorded sequence: ");
        if (clickCounter > 31) {
          clickCounter = 31;
        }
        for (int i = 0; i < clickCounter - 1; i++) {
          SUPLA_LOG_DEBUG("%d", currentSequence.data[i]);
        }

        int matchSequenceSize = 0;
        for (; matchSequenceSize < 30; matchSequenceSize++) {
          if (matchSequence.data[matchSequenceSize] == 0) {
            break;
          }
        }
        if (matchSequenceSize != clickCounter - 1) {
          SUPLA_LOG_DEBUG("Sequence size doesn't match");
          runAction(ON_SEQUENCE_DOESNT_MATCH);
        } else {
          bool match = true;
          for (int i = 0; i < clickCounter - 1; i++) {
            unsigned int marginValue = calculateMargin(matchSequence.data[i]);
            if (!(matchSequence.data[i] - marginValue <=
                      currentSequence.data[i] &&
                  matchSequence.data[i] + marginValue >=
                      currentSequence.data[i])) {
              match = false;
              break;
            }
          }
          if (match) {
            SUPLA_LOG_DEBUG("Sequence match");
            runAction(ON_SEQUENCE_MATCH);
          } else {
            SUPLA_LOG_DEBUG("Sequence doesn't match");
            runAction(ON_SEQUENCE_DOESNT_MATCH);
          }
        }
        clickCounter = 0;
      }
    }
  }
}

unsigned int Supla::Control::SequenceButton::calculateMargin(
    unsigned int value) {
  unsigned int result = margin * value;
  if (result < 20) {
    result = 20;
  }
  return result;
}

void Supla::Control::SequenceButton::setMargin(float newMargin) {
  margin = newMargin;
  if (margin < 0) {
    margin = 0;
  } else if (margin > 1) {
    margin = 1;
  }
}

void Supla::Control::SequenceButton::setSequence(uint16_t *sequence) {
  uint16_t maxValue = 0;
  for (int i = 0; i < SEQUENCE_MAX_SIZE; i++) {
    matchSequence.data[i] = sequence[i];
    if (sequence[i] > maxValue) {
      maxValue = sequence[i];
    }
  }
  maxValue *= 1.5;
  if (maxValue < 500) {
    maxValue = 500;
  }
  longestSequenceTimeDeltaWithMargin = maxValue;
}

void Supla::Control::SequenceButton::getLastRecordedSequence(
    uint16_t *sequence) const {
  memcpy(sequence, currentSequence.data, sizeof(uint16_t[SEQUENCE_MAX_SIZE]));
}

