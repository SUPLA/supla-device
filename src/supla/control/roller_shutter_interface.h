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

#ifndef SRC_SUPLA_CONTROL_ROLLER_SHUTTER_INTERFACE_H_
#define SRC_SUPLA_CONTROL_ROLLER_SHUTTER_INTERFACE_H_

#include "../action_handler.h"
#include "../channel_element.h"

#define UNKNOWN_POSITION   -1
#define STOP_POSITION      -2
#define MOVE_UP_POSITION   -3
#define MOVE_DOWN_POSITION -4
#define STOP_REQUEST       -5
#define RS_DEFAULT_OPERATION_TIMEOUT_MS 60000

namespace Supla {

namespace Control {

class Button;

enum class Directions : uint8_t { STOP_DIR = 0, DOWN_DIR = 1, UP_DIR = 2 };

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

struct TiltConfig {
  uint32_t tiltingTime = 0;
  uint16_t tilt0Angle = 0;    // 0 - 180 - degree corresponding to tilt 0
  uint16_t tilt100Angle = 0;  // 0 - 180 - degree corresponding to tilt 100
  uint8_t tiltControlType =
      SUPLA_TILT_CONTROL_TYPE_UNKNOWN;  // SUPLA_TILT_CONTROL_TYPE_

  void clear();
};
#pragma pack(pop)

class RollerShutterInterface : public ChannelElement, public ActionHandler {
 public:
  /**
   * Constructor.
   * Changing of tilt functions will breake state storage. So make sure
   * that those functions are enabled before the first device startup.
   * You can enable them in existing devices as well, but make sure that
   * you don't have anything important in state storage (i.e. Electricity
   * Meter data).
   *
   * @param tiltFunctionsEnabled true if tilt functions should be added
   */
  explicit RollerShutterInterface(bool tiltFunctionsEnabled = false);

  /**
   * Destructor
   */
  virtual ~RollerShutterInterface();

  RollerShutterInterface(const RollerShutterInterface &) = delete;
  RollerShutterInterface &operator=(const RollerShutterInterface &) = delete;

  /**
   * Add tilt functions (facade blinds, vertical blinds)
   * Changing of tilt functions will breake state storage. So make sure
   * that those functions are enabled before the first device startup.
   * You can enable them in existing devices as well, but make sure that
   * you don't have anything important in state storage (i.e. Electricity
   * Meter data).
   *
   */
  void addTiltFunctions();

  /**
   * Check if tilt functions are supported
   *
   * @return true if tilt functions are supported
   */
  bool isTiltFunctionsSupported() const;

  /**
   * Check if tilt function is currently enabled
   *
   * @return true if tilt function is enabled
   */
  bool isTiltFunctionEnabled() const;

  /**
   * Check if tilting is configured (time and control modes are set and
   * isTiltFunctionEnabled is true)
   *
   * @return true if tilting is configured
   */
  bool isTiltConfigured() const;

  /**
   * Check if top position (and tilt if applicable) is reached
   *
   * @return true if top position is reached
   */
  bool isTopReached() const;

  /**
   * Check if bottom position (and tilt if applicable) is reached
   *
   * @return true if bottom position is reached
   */
  bool isBottomReached() const;

  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  void handleAction(int event, int action) override;
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local = false) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;

  // Method is used by external integrations to prepare TSD_SuplaChannelNewValue
  // value for specific channel type (i.e. to prefill durationMS field when
  // required)
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;

  virtual void close();         // Sets target position to 100%
  virtual void open();          // Sets target position to 0%
  virtual void stop();          // Stop motor
  virtual void moveUp();    // start opening roller shutter regardless of its
                            // position (keep motor going up)
  virtual void moveDown();  // starts closing roller shutter regardless of its
                            // position (keep motor going down)

  virtual void setTargetPosition(int newPosition,
                                 int newTilt = UNKNOWN_POSITION);
  /**
   * Set current roller shutter/facade blind position (and tilt)
   * (0 = open; 100 = closed)
   *
   * @param newPosition
   * @param newTilt
   */
  void setCurrentPosition(int newPosition, int newTilt = UNKNOWN_POSITION);
  void setNotCalibrated();
  // Sets calibration ongoing flag, by setting calibration timeout.
  // calibrationTime = 1 is used to indicate ongoing calibration for
  // devices without explicit calibration time setting
  void setCalibrationOngoing(int calibrationTime = 1);
  void setCalibrationFinished();

  /**
   * Get current roller shutter position
   *
   * @return 0-100 (0 = open; 100 = closed); -1 if unknown
   */
  int getCurrentPosition() const;

  /**
   * Get current tilt position
   *
   * @return 0-100 (0 = final position after move up; 100 = final position after
   *         move down); -1 if unknown or N/A
   */
  int getCurrentTilt() const;

  /**
   * Get target roller shutter position
   *
   * @return 0-100 (0 = open; 100 = closed); -1 if unknown, -2 if stop,
   *         -3 if move up, -4 if move down
   */
  int getTargetPosition() const;

  /**
   * Get target tilt position
   *
   * @return 0-100 (0 = final position after move up; 100 = final position after
   *         move down); -1 if unknown or N/A
   */
  int getTargetTilt() const;

  /**
   * Get current roller shutter movement direction
   *
   * @return int value of enum Supla::Control::Directions
   */
  int getCurrentDirection() const;

  void configComfortUpValue(uint8_t position);
  void configComfortDownValue(uint8_t position);
  void configComfortUpTiltValue(uint8_t position);
  void configComfortDownTiltValue(uint8_t position);

  void onInit() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void saveConfig();
  void onLoadState() override;
  void onSaveState() override;
  void purgeConfig() override;
  void iterateAlways() override;

  uint32_t getClosingTimeMs() const;
  uint32_t getOpeningTimeMs() const;
  uint32_t getTiltingTimeMs() const;
  uint32_t getTiltControlType() const;

  void attach(Supla::Control::Button *up, Supla::Control::Button *down);
  void attach(Supla::Control::Button *button, bool upButton, bool asInternal);

  virtual void triggerCalibration();
  void setCalibrationNeeded();
  bool isCalibrationRequested() const;
  bool isCalibrated() const;

  /**
   * Enable/disable motor upside down configuration option. If enabled,
   * then motor upside down can be set in configuration.
   * If disabled, then motor upside down can not be set in configuration.
   *
   * @param enable
   */
  void setRsConfigMotorUpsideDownEnabled(bool enable);

  /**
   * Enable/disable buttons upside down configuration option. If enabled,
   * then buttons upside down can be set in configuration.
   * If disabled, then buttons upside down can not be set in configuration.
   *
   * @param enable
   */
  void setRsConfigButtonsUpsideDownEnabled(bool enable);

  /**
   * Enable/disable time margin configuration option. If enabled,
   * then time margin can be set in configuration.
   * If disabled, then time margin can not be set in configuration.
   *
   * @param enable
   */
  void setRsConfigTimeMarginEnabled(bool enable);

  /**
   * Set motor upside down value. Works only if motor upside down
   * configuration option is enabled.
   *
   * @param value 0 - not set/not used, 1 - false, 2 - true
   */
  void setRsConfigMotorUpsideDownValue(uint8_t value);

  /**
   * Set buttons upside down value. Works only if buttons upside down
   * configuration option is enabled.
   *
   * @param value 0 - not set/not used, 1 - false, 2 - true
   */
  void setRsConfigButtonsUpsideDownValue(uint8_t value);

  /**
   * Set time margin value. Works only if time margin configuration option
   * is enabled.
   *
   * @param value -1 (use device specific default), 0 (not set/not used),
   * 1 (no margin), > 1 (51% of opening/closing time added on extreme
   * positions)
   */
  void setRsConfigTimeMarginValue(int8_t value);

  uint8_t getMotorUpsideDown() const;
  uint8_t getButtonsUpsideDown() const;
  int8_t getTimeMargin() const;

  static void setRsStorageSaveDelay(int delayMs);

  virtual bool inMove();
  virtual bool isCalibrationInProgress() const;
  void startCalibration(uint32_t timeMs);
  void stopCalibration();
  bool isCalibrationFailed() const;
  bool isCalibrationLost() const;
  bool isMotorProblem() const;

  bool isFunctionSupported(int32_t channelFunction) const;
  bool isAutoCalibrationSupported() const;

  void setOpenCloseTime(uint32_t newClosingTimeMs, uint32_t newOpeningTimeMs);
  void setTiltingTime(uint32_t newTiltingTimeMs, bool local = true);
  void setTiltControlType(uint8_t newTiltControlType, bool local = true);

  void setCalibrationFailed(bool value);
  void setCalibrationLost(bool value);
  void setMotorProblem(bool value);

 protected:
  struct ButtonListElement {
    Supla::Control::Button *button = nullptr;
    bool asInternal = true;
    bool upButton = true;
    ButtonListElement *next = nullptr;
  };

  /**
   * Configure additional buttons for roller shutter
   *
   * @param button Button
   * @param upButton true if button is up button, false if down
   * @param asInternal true if buttons are internal (they will be inverted by
   *                   upsideDown config)
   */
  void setupButtonActions(Supla::Control::Button *button,
                               bool upButton, bool asInternal);


  bool lastDirectionWasOpen() const;
  bool lastDirectionWasClose() const;

  // returns true, when opening/closing time is setable from config
  bool isTimeSettingAvailable() const;
  bool getCalibrate() const;
  void setCalibrate(bool value);

  void printConfig() const;
  uint32_t getTimeMarginValue(uint32_t fullTime) const;

  uint8_t flags = 0;

  uint8_t comfortDownValue = 20;
  uint8_t comfortUpValue = 80;
  uint8_t comfortUpTiltValue = 0;
  uint8_t comfortDownTiltValue = 100;

  Directions currentDirection = Directions::STOP_DIR;  // stop, up, down
  Directions lastDirection = Directions::STOP_DIR;

  int16_t currentPosition =
      UNKNOWN_POSITION;  // 0 - open; 10000 - closed, in 0.01 units
  int16_t currentTilt =
      UNKNOWN_POSITION;  // 0 - opening position
                         // 10000 - closing position, in 0.01 units
  int8_t targetPosition = STOP_POSITION;         // 0-100
  int8_t targetTilt = UNKNOWN_POSITION;          // 0-100
  int16_t lastPositionBeforeMovement = UNKNOWN_POSITION;  // 0-100
  int16_t lastTiltBeforeMovement = UNKNOWN_POSITION;      // 0-100
  bool newTargetPositionAvailable = false;

  RollerShutterConfig rsConfig;
  TiltConfig tiltConfig;

  ButtonListElement *buttonList = nullptr;

  uint32_t closingTimeMs = 0;
  uint32_t openingTimeMs = 0;
  // Calibration time may be used also as indication of ongoing calibration,
  // but without information about calibration lenght. In general:
  // calibrationTime > 0 means that there is ongoing calibration
  uint32_t calibrationTime = 0;
  uint32_t lastUpdateTime = 0;

  static int16_t rsStorageSaveDelay;
};

}  // namespace Control
}  // namespace Supla


#endif  // SRC_SUPLA_CONTROL_ROLLER_SHUTTER_INTERFACE_H_
