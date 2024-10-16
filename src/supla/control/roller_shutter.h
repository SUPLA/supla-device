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

#ifndef SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_
#define SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_

#include <stdint.h>

#include "../action_handler.h"
#include "../channel_element.h"

#define UNKNOWN_POSITION   -1
#define STOP_POSITION      -2
#define MOVE_UP_POSITION   -3
#define MOVE_DOWN_POSITION -4

namespace Supla {

class Io;

namespace Control {

class Button;

enum Directions { STOP_DIR = 0, DOWN_DIR = 1, UP_DIR = 2 };

#pragma pack(push, 1)
struct RollerShutterConfig {
  uint8_t motorUpsideDown = 1;    // 0 - not set/not used, 1 - false, 2 - true
  uint8_t buttonsUpsideDown = 1;  // 0 - not set/not used, 1 - false, 2 - true
  int8_t timeMargin = -1;  // -1 default (device specific), 0 - not set/not used
                           // 1 - no margin,
                           // > 1 - 51% of opening/closing time added on extreme
                           // positions - value should be decremented by 1.
  uint8_t visualizationType = 0;  // 0 - default, other values depends on
                                    // Cloud and App support
};
#pragma pack(pop)

class RollerShutter : public ChannelElement, public ActionHandler {
 public:
  RollerShutter(Supla::Io *io, int pinUp, int pinDown, bool highIsOn = true);
  RollerShutter(int pinUp, int pinDown, bool highIsOn = true);

  RollerShutter(const RollerShutter &) = delete;
  RollerShutter &operator=(const RollerShutter &) = delete;

  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  void handleAction(int event, int action) override;
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) override;
  uint8_t applyChannelConfig(TSD_ChannelConfig *result,
                             bool local = false) override;
  void fillChannelConfig(void *channelConfig, int *size) override;

  // Method is used by external integrations to prepare TSD_SuplaChannelNewValue
  // value for specific channel type (i.e. to prefill durationMS field when
  // required)
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;

  void close();         // Sets target position to 100%
  void open();          // Sets target position to 0%
  virtual void stop();  // Stop motor
  void moveUp();    // start opening roller shutter regardless of its position
                    // (keep motor going up)
  void moveDown();  // starts closing roller shutter regardless of its position
                    // (keep motor going down)
  void setTargetPosition(int newPosition);
  int getCurrentPosition() const;
  // Get current roller shutter movement direction. Returns int value of
  // enum Supla::Control::Directions
  int getCurrentDirection() const;

  void configComfortUpValue(uint8_t position);
  void configComfortDownValue(uint8_t position);

  void onInit() override;
  void onTimer() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void saveConfig();
  void onLoadState() override;
  void onSaveState() override;

  uint32_t getClosingTimeMs() const;
  uint32_t getOpeningTimeMs() const;

  void attach(Supla::Control::Button *up, Supla::Control::Button *down);

  void triggerCalibration();
  void setCalibrationNeeded();
  bool isCalibrationRequested() const;
  bool isCalibrated() const;

  void setRsConfigMotorUpsideDownEnabled(bool enable);
  void setRsConfigButtonsUpsideDownEnabled(bool enable);
  void setRsConfigTimeMarginEnabled(bool enable);

  void setRsConfigMotorUpsideDownValue(uint8_t value);
  void setRsConfigButtonsUpsideDownValue(uint8_t value);
  void setRsConfigTimeMarginValue(int8_t value);

  uint8_t getMotorUpsideDown() const;
  uint8_t getButtonsUpsideDown() const;
  int8_t getTimeMargin() const;

  static void setRsStorageSaveDelay(int delayMs);
  virtual bool inMove();

  bool isFunctionSupported(int32_t channelFunction) const;
  bool isAutoCalibrationSupported() const;

  void setOpenCloseTime(uint32_t newClosingTimeMs, uint32_t newOpeningTimeMs);

 protected:
  virtual void stopMovement();
  virtual void relayDownOn();
  virtual void relayUpOn();
  virtual void relayDownOff();
  virtual void relayUpOff();
  virtual void startClosing();
  virtual void startOpening();
  virtual void switchOffRelays();

  bool lastDirectionWasOpen() const;
  bool lastDirectionWasClose() const;

  void printConfig() const;
  void setupButtonActions();
  uint32_t getTimeMarginValue(uint32_t fullTime) const;

  uint32_t closingTimeMs = 0;
  uint32_t openingTimeMs = 0;
  int16_t pinUp = -1;
  int16_t pinDown = -1;

  uint32_t lastMovementStartTime = 0;
  uint32_t doNothingTime = 0;
  uint32_t calibrationTime = 0;
  Supla::Io *io = nullptr;

  uint32_t operationTimeoutMs = 0;

  bool calibrate =
      true;  // set to true when new closing/opening time is given -
             // calibration is done to sync roller shutter position

  uint8_t comfortDownValue = 20;
  uint8_t comfortUpValue = 80;

  bool newTargetPositionAvailable = false;

  bool highIsOn = true;

  uint8_t currentDirection = STOP_DIR;  // stop, up, down
  uint8_t lastDirection = STOP_DIR;

  int8_t currentPosition = UNKNOWN_POSITION;  // 0 - closed; 100 - opened
  int8_t targetPosition = STOP_POSITION;      // 0-100
  int8_t lastPositionBeforeMovement = UNKNOWN_POSITION;  // 0-100

  RollerShutterConfig rsConfig;

  Supla::Control::Button *upButton = nullptr;
  Supla::Control::Button *downButton = nullptr;

  static int16_t rsStorageSaveDelay;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_
