// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_
#define SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_

#include <supla/io.h>

#include "roller_shutter_interface.h"

namespace Supla {

namespace Io {
class Base;
}

namespace Html {
class RollerShutterParameters;
}

namespace Control {

class RollerShutter : public RollerShutterInterface {
 public:
  friend class Supla::Html::RollerShutterParameters;
   /**
    * Constructor.
    *
    * @param io Supla::Io inteface (if other than default)
    * @param pinUp GPIO pin used for moving up
    * @param pinDown GPIO pin used for moving down
    * @param highIsOn true for active high
    * @param tiltFunctionsEnabled true to enable tilt functions (changing this
    *        value will reset state storage)
    */
  RollerShutter(Supla::Io::Base *io,
                int pinUp,
                int pinDown,
                bool highIsOn = true,
                bool tiltFunctionsEnabled = false);
  /**
   * Constructor.
   *
   * @param pinUp GPIO pin used for moving up
   * @param pinDown GPIO pin used for moving down
   * @param highIsOn true for active high
   * @param tiltFunctionsEnabled true to enable tilt functions (changing this
   *        value will reset state storage)
   */
  RollerShutter(int pinUp = -1,
                int pinDown = -1,
                bool highIsOn = true,
                bool tiltFunctionsEnabled = false);
  /**
   * Constructor.
   *
   * @param pinUp GPIO pin used for moving up
   * @param pinDown GPIO pin used for moving down
   * @param tiltFunctionsEnabled true to enable tilt functions (changing this
   *        value will reset state storage)
   */
  RollerShutter(Supla::Io::IoPin pinUp,
                Supla::Io::IoPin pinDown,
                bool tiltFunctionsEnabled = false);

  void onInit() override;
  void onTimer() override;

  void setPinUp(int pin);
  void setPinDown(int pin);

  void setTargetPosition(int newPosition,
                         int newTilt = UNKNOWN_POSITION) override;

 protected:
  RollerShutter(Supla::Io::IoPin pinUp,
                Supla::Io::IoPin pinDown,
                bool tiltFunctionsEnabled,
                Supla::Channel &externalChannel,
                ElementMode mode);

  virtual void stopMovement();
  virtual void relayDownOn();
  virtual void relayUpOn();
  virtual void relayDownOff();
  virtual void relayUpOff();
  virtual void startClosing();
  virtual void startOpening();
  virtual void switchOffRelays();
  void calculateCurrentPositionAndTilt();

  void initGpio(const Supla::Io::IoPin &pin);

  Supla::Io::IoPin pinUp;
  Supla::Io::IoPin pinDown;

  uint32_t lastMovementStartTime = 0;
  uint32_t doNothingTime = 0;

  uint32_t operationTimeoutMs = 0;
  bool invalidTiltConfigurationWarningLogged = false;
  bool invalidTiltConfigurationFallbackActive = false;
  bool invalidTiltOnlyRuntimeWarningLogged = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_
