// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

