// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_BUTTON_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_BUTTON_PARSED_H_

#include <supla/control/button.h>
#include <supla/io.h>
#include <supla/sensor/sensor_parsed.h>

#include <atomic>
#include <string>

namespace Supla {
namespace Control {

namespace Internal {
// Virtual GPIO input fed from a parser. The parser is read from the main
// loop, while Button reads the input from the timer thread, so the value is
// kept in an atomic.
class ParsedButtonInput : public Supla::Io::Base {
 public:
  void customPinMode(int, uint8_t, uint8_t) override {}
  int customDigitalRead(int, uint8_t) override {
    return state.load();
  }

  std::atomic<int> state{0};
};

// Holder is a separate base class, so the input exists before Button is
// constructed with a pointer to it.
struct ParsedButtonInputHolder {
  ParsedButtonInput parsedInput;
};
}  // namespace Internal

/**
 * Button which reads its input state from a parser (File, Cmd, MQTT...)
 * instead of a GPIO. It behaves like a regular Supla::Control::Button, so it
 * can be attached to roller shutters and relays.
 */
class ButtonParsed : private Internal::ParsedButtonInputHolder,
                     public Button,
                     public Sensor::SensorParsedBase {
 public:
  ButtonParsed(Supla::Parser::Parser *parser, const std::string &name);

  void setInvertLogic(bool invert);

  void onInit() override;
  void iterateAlways() override;

  void setInitialCaption(const std::string &) override {}
  Supla::Channel *getChannel() override {
    return nullptr;
  }
  const Supla::Channel *getChannel() const override {
    return nullptr;
  }
  Supla::Channel *getSecondaryChannel() override {
    return nullptr;
  }
  const Supla::Channel *getSecondaryChannel() const override {
    return nullptr;
  }

 private:
  void refreshInput();

  std::string name;
  bool invertLogic = false;
  int lastParsedState = -2;
};

}  // namespace Control
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_BUTTON_PARSED_H_
