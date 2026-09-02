// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_PIN_STATUS_LED_H_
#define SRC_SUPLA_CONTROL_PIN_STATUS_LED_H_

#include "../element.h"
#include "../io.h"

namespace Supla {

namespace Io {
class Base;
}

namespace Control {
class PinStatusLed : public Element {
 public:
  explicit PinStatusLed(Supla::Io::IoPin srcPin, Supla::Io::IoPin outPin);
  explicit PinStatusLed(Supla::Io::IoPin srcPin,
                        Supla::Io::IoPin outPin,
                        bool invert);
  PinStatusLed(Supla::Io::Base *ioSrc,
               Supla::Io::Base *ioOut,
               uint8_t srcPin,
               uint8_t outPin,
               bool invert = false);
  PinStatusLed(uint8_t srcPin, uint8_t outPin, bool invert = false);

  void onInit() override;
  void iterateAlways() override;
  void onTimer() override;

  void setInvertedLogic(bool invertedLogic);
  void setWorkOnTimer(bool workOnTimer);

 protected:
  void updatePin();

  Supla::Io::IoPin srcPin;
  Supla::Io::IoPin outPin;
  bool workOnTimer = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_PIN_STATUS_LED_H_
