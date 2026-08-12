// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * This class allows to control roller shutters with 3 buttons: up, down, stop
 */

#ifndef SRC_SUPLA_CONTROL_TRIPPLE_BUTTON_ROLLER_SHUTTER_H_
#define SRC_SUPLA_CONTROL_TRIPPLE_BUTTON_ROLLER_SHUTTER_H_

#include <supla/io.h>

#include "bistable_roller_shutter.h"

namespace Supla {
namespace Io {
class Base;
}  // namespace Io

namespace Control {
class TrippleButtonRollerShutter : public BistableRollerShutter {
 public:
  TrippleButtonRollerShutter(Supla::Io::Base *io,
                             int pinUp,
                             int pinDown,
                             int pinStop,
                             bool highIsOn = true);
  TrippleButtonRollerShutter(int pinUp,
                             int pinDown,
                             int pinStop,
                             bool highIsOn = true);
  TrippleButtonRollerShutter(Supla::Io::IoPin pinUp,
                             Supla::Io::IoPin pinDown,
                             Supla::Io::IoPin pinStop);
  virtual ~TrippleButtonRollerShutter();

  void onInit() override;

 protected:
  void stopMovement() override;
  void switchOffRelays() override;
  bool inMove() override;
  virtual void relayStopOn();
  virtual void relayStopOff();

  Supla::Io::IoPin pinStop;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_TRIPPLE_BUTTON_ROLLER_SHUTTER_H_
