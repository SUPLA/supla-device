// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_INTERNAL_PIN_OUTPUT_H_
#define SRC_SUPLA_CONTROL_INTERNAL_PIN_OUTPUT_H_

#include <supla/io.h>

#include "../action_handler.h"
#include "../element.h"
#include "../local_action.h"
#include "output_interface.h"

#define STATE_ON_INIT_OFF 0
#define STATE_ON_INIT_ON  1

namespace Supla {

namespace Io {
class Base;
}

namespace Control {
class InternalPinOutput : public Element,
                          public ActionHandler,
                          public LocalAction,
                          public OutputInterface {
 public:
  explicit InternalPinOutput(Supla::Io::IoPin outPin);
  explicit InternalPinOutput(Supla::Io::IoPin outPin, bool highIsOn);
  explicit InternalPinOutput(Supla::Io::Base *io,
                             int pin,
                             bool highIsOn = true);
  explicit InternalPinOutput(int pin, bool highIsOn = true);

  virtual InternalPinOutput &setDefaultStateOn();
  virtual InternalPinOutput &setDefaultStateOff();
  virtual InternalPinOutput &setDurationMs(_supla_int_t duration);

  virtual uint8_t pinOnValue();
  virtual uint8_t pinOffValue();
  virtual void turnOn(_supla_int_t duration = 0);
  virtual void turnOff(_supla_int_t duration = 0);
  virtual bool isOn();
  virtual void toggle(_supla_int_t duration = 0);

  void handleAction(int event, int action) override;

  void onInit() override;
  void iterateAlways() override;

  int getOutputValue() const override;
  void setOutputValue(int value) override;
  bool isOnOffOnly() const override;

 protected:
  Supla::Io::IoPin outPin;
  int8_t lastOutputValue = 0;
  int8_t stateOnInit = STATE_ON_INIT_OFF;
  uint32_t durationMs = 0;
  uint32_t storedTurnOnDurationMs = 0;
  uint32_t durationTimestamp = 0;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_INTERNAL_PIN_OUTPUT_H_
