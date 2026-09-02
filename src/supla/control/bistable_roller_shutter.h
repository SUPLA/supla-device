// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_BISTABLE_ROLLER_SHUTTER_H_
#define SRC_SUPLA_CONTROL_BISTABLE_ROLLER_SHUTTER_H_

#include <supla/io.h>

#include "roller_shutter.h"

namespace Supla {
namespace Io {
class Base;
}

namespace Control {
class BistableRollerShutter : public RollerShutter {
 public:
  BistableRollerShutter(Supla::Io::Base *io,
                        int pinUp,
                        int pinDown,
                        bool highIsOn = true);
  BistableRollerShutter(int pinUp, int pinDown, bool highIsOn = true);
  BistableRollerShutter(Supla::Io::IoPin pinUp, Supla::Io::IoPin pinDown);

  void onTimer() override;

 protected:
  void stopMovement() override;
  void relayDownOn() override;
  void relayUpOn() override;
  void relayUpOff() override;
  void relayDownOff() override;

  bool activeBiRelay = false;
  uint32_t toggleTime = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BISTABLE_ROLLER_SHUTTER_H_
