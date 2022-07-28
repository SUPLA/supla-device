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

#include <supla/time.h>

#include "button.h"

Supla::Control::Button::Button(int pin, bool pullUp, bool invertLogic)
    : SimpleButton(pin, pullUp, invertLogic),
      holdTimeMs(0),
      repeatOnHoldMs(0),
      multiclickTimeMs(0),
      lastStateChangeMs(0),
      clickCounter(0),
      holdSend(0),
      bistable(false) {
}

void Supla::Control::Button::onTimer() {
  uint64_t timeDelta = millis() - lastStateChangeMs;
  bool stateChanged = false;
  int stateResult = state.update();
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
    if (stateResult == TO_PRESSED || bistable) {
      clickCounter++;
    }
  }

  if (!stateChanged && lastStateChangeMs) {
    if (!bistable && stateResult == PRESSED) {
      if (clickCounter <= 1 && holdTimeMs > 0 &&
          timeDelta > (holdTimeMs + holdSend * repeatOnHoldMs) &&
          (repeatOnHoldMs == 0 ? !holdSend : true)) {
        runAction(ON_HOLD);
        ++holdSend;
      }
    } else if ((bistable || stateResult == RELEASED)) {
      if (multiclickTimeMs == 0) {
        holdSend = 0;
        clickCounter = 0;
      }
      if (multiclickTimeMs > 0 && timeDelta > multiclickTimeMs) {
        if (holdSend == 0) {
          switch (clickCounter) {
            case 1:
              runAction(ON_CLICK_1);
              break;
            case 2:
              runAction(ON_CLICK_2);
              break;
            case 3:
              runAction(ON_CLICK_3);
              break;
            case 4:
              runAction(ON_CLICK_4);
              break;
            case 5:
              runAction(ON_CLICK_5);
              break;
            case 6:
              runAction(ON_CLICK_6);
              break;
            case 7:
              runAction(ON_CLICK_7);
              break;
            case 8:
              runAction(ON_CLICK_8);
              break;
            case 9:
              runAction(ON_CLICK_9);
              break;
            case 10:
              runAction(ON_CLICK_10);
              break;
          }
          if (clickCounter >= 10) {
            runAction(ON_CRAZY_CLICKER);
          }
        } else {
          switch (clickCounter) {
            // for LONG_CLICK counter was incremented once by ON_HOLD
            case 1:
              runAction(ON_LONG_CLICK_0);
              break;
            case 2:
              runAction(ON_LONG_CLICK_1);
              break;
            case 3:
              runAction(ON_LONG_CLICK_2);
              break;
            case 4:
              runAction(ON_LONG_CLICK_3);
              break;
            case 5:
              runAction(ON_LONG_CLICK_4);
              break;
            case 6:
              runAction(ON_LONG_CLICK_5);
              break;
            case 7:
              runAction(ON_LONG_CLICK_6);
              break;
            case 8:
              runAction(ON_LONG_CLICK_7);
              break;
            case 9:
              runAction(ON_LONG_CLICK_8);
              break;
            case 10:
              runAction(ON_LONG_CLICK_9);
              break;
            case 11:
              runAction(ON_LONG_CLICK_10);
              break;
          }
        }
        holdSend = 0;
        clickCounter = 0;
      }
    }
  }
}

void Supla::Control::Button::setHoldTime(unsigned int timeMs) {
  holdTimeMs = timeMs;
  if (bistable) {
    holdTimeMs = 0;
  }
}

void Supla::Control::Button::setMulticlickTime(unsigned int timeMs,
                                               bool bistableButton) {
  multiclickTimeMs = timeMs;
  bistable = bistableButton;
  if (bistable) {
    holdTimeMs = 0;
  }
}

void Supla::Control::Button::repeatOnHoldEvery(unsigned int timeMs) {
  repeatOnHoldMs = timeMs;
}

bool Supla::Control::Button::isBistable() const {
  return bistable;
}
