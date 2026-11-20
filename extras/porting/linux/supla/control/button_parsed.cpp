// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "button_parsed.h"

#include <supla/log_wrapper.h>

#include <string>

using Supla::Control::ButtonParsed;

ButtonParsed::ButtonParsed(Supla::Parser::Parser *parser,
                           const std::string &name)
    : Button(Supla::Io::IoPin(0, &parsedInput)),
      SensorParsedBase(parser),
      name(name) {
  // Button type and timings come from the YAML config, not from storage.
  dontUseOnLoadConfig();
}

void ButtonParsed::setInvertLogic(bool invert) {
  invertLogic = invert;
}

void ButtonParsed::onInit() {
  // Read the real input state before Button initializes its previous state,
  // so an already active input doesn't generate a press on startup.
  refreshInput();
  Button::onInit();
}

void ButtonParsed::iterateAlways() {
  refreshInput();
  Button::iterateAlways();
}

void ButtonParsed::refreshInput() {
  // The parser limits how often its source is really read (refresh_time_ms).
  int state = getStateValue(false);
  if (state != lastParsedState) {
    if (state < 0) {
      SUPLA_LOG_WARNING("Button[%d] \"%s\": invalid input, treated as released",
                        getButtonNumber(),
                        name.c_str());
    } else {
      SUPLA_LOG_DEBUG("Button[%d] \"%s\": input %d",
                      getButtonNumber(),
                      name.c_str(),
                      state);
    }
    lastParsedState = state;
  }

  bool active = state == 1;
  if (state < 0) {
    active = false;  // never keep a button "held" on a lost source
  } else if (invertLogic) {
    active = !active;
  }
  parsedInput.state.store(active ? HIGH : LOW);
}
