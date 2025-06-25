/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "button_aggregator.h"

#include <supla/time.h>
#include <supla/log_wrapper.h>

using Supla::Control::ButtonAggregator;

ButtonAggregator::ButtonAggregator() : Supla::Control::Button(nullptr, -1) {
  dontUseOnLoadConfig();
}

ButtonAggregator::~ButtonAggregator() {
}

void ButtonAggregator::onTimer() {
  if (stateChanged) {
    stateChanged = false;
    if (pressCount == buttonCount) {
      allPressedTimestamp = millis();
    } else {
      allPressedTimestamp = 0;
    }
    return;
  }

  if (allPressedTimestamp != 0 && millis() - allPressedTimestamp > 5000) {
    SUPLA_LOG_DEBUG("ButtonAggregator: triggering ON_HOLD");
    runAction(ON_HOLD);
    pressCount = 0;
    allPressedTimestamp = 0;
  }
}

void ButtonAggregator::handleAction(int, int action) {
  if (action == Supla::ON_PRESS) {
    pressCount++;
    SUPLA_LOG_DEBUG("ButtonAggregator: pressCount++: %d", pressCount);
    if (pressCount >= buttonCount) {
      pressCount = buttonCount;
      for (int i = 0; i < BUTTON_AGGREGATOR_MAX_BUTTONS; i++) {
        if (buttons[i] != nullptr) {
          buttons[i]->waitForRelease();
        }
      }
    }
    stateChanged = true;
  } else if (action == Supla::ON_RELEASE) {
    pressCount--;
    SUPLA_LOG_DEBUG("ButtonAggregator: pressCount--: %d", pressCount);
    if (pressCount < 0) {
      pressCount = 0;
    }
    stateChanged = true;
  }
}

bool ButtonAggregator::addButton(Supla::Control::Button* button) {
  for (int i = 0; i < BUTTON_AGGREGATOR_MAX_BUTTONS; i++) {
    if (buttons[i] == button) {
      return true;
    }
    if (buttons[i] == nullptr) {
      buttons[i] = button;
      buttonCount++;
      button->addAction(Supla::ON_PRESS, this, Supla::ON_PRESS, true);
      button->addAction(Supla::ON_RELEASE, this, Supla::ON_RELEASE, true);
      SUPLA_LOG_DEBUG("ButtonAggregator: added button %d", i);
      return true;
    }
  }
  return false;
}

