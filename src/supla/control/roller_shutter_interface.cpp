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

#include "roller_shutter_interface.h"

#include <supla/log_wrapper.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/io.h>
#include <supla/control/button.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/local_action.h>
#include <supla/actions.h>

using Supla::Control::RollerShutterInterface;

int16_t RollerShutterInterface::rsStorageSaveDelay = 5000;

#pragma pack(push, 1)
struct RollerShutterStateData {
  uint32_t closingTimeMs;
  uint32_t openingTimeMs;
  int8_t currentPosition;  // 0 - closed; 100 - opened
};
#pragma pack(pop)

RollerShutterInterface::RollerShutterInterface() {
  channel.setType(SUPLA_CHANNELTYPE_RELAY);
  channel.setDefault(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  channel.setFuncList(SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER |
                      SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW |
                      SUPLA_BIT_FUNC_TERRACE_AWNING |
                      SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR |
                      SUPLA_BIT_FUNC_CURTAIN |
                      SUPLA_BIT_FUNC_PROJECTOR_SCREEN);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
}

bool RollerShutterInterface::isFunctionSupported(
    int32_t channelFunction) const {
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
      return getChannel()->getFuncList() &
             SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER;
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
      return getChannel()->getFuncList() &
             SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW;
    }
    case SUPLA_CHANNELFNC_TERRACE_AWNING: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_TERRACE_AWNING;
    }
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR: {
      return getChannel()->getFuncList() &
             SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR;
    }
    case SUPLA_CHANNELFNC_CURTAIN: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_CURTAIN;
    }
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_PROJECTOR_SCREEN;
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND: {
      return getChannel()->getFuncList() &
             SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND;
    }
    case SUPLA_CHANNELFNC_VERTICAL_BLIND: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_VERTICAL_BLIND;
    }
    default: {
      return false;
    }
  }
}

void RollerShutterInterface::onInit() {
  setupButtonActions();
  iterateAlways();
}

void RollerShutterInterface::setupButtonActions() {
  if (upButton) {
    upButton->onInit();  // make sure button was initialized
    if (upButton->isMonostable()) {
      upButton->addAction(Supla::INTERNAL_BUTTON_MOVE_UP_OR_STOP,
                          this,
                          Supla::CONDITIONAL_ON_PRESS);
    } else if (upButton->isBistable()) {
      upButton->addAction(Supla::INTERNAL_BUTTON_MOVE_UP_OR_STOP,
                          this,
                          Supla::CONDITIONAL_ON_CHANGE);
    }
  }

  if (downButton) {
    downButton->onInit();  // make sure button was initialized
    if (downButton->isMonostable()) {
      downButton->addAction(Supla::INTERNAL_BUTTON_MOVE_DOWN_OR_STOP,
                            this,
                            Supla::CONDITIONAL_ON_PRESS);
    } else if (downButton->isBistable()) {
      downButton->addAction(Supla::INTERNAL_BUTTON_MOVE_DOWN_OR_STOP,
                            this,
                            Supla::CONDITIONAL_ON_CHANGE);
    }
  }
}

/*
 * Protocol:
 * value[0]:
 *  0 - stop
 *  1 - down
 *  2 - up
 *  10 - 110 - 0% - 100%
 *
 * time is send in 0.1 s. i.e. 105 -> 10.5 s
 * time * 100 = gives time in ms
 *
 */

int32_t RollerShutterInterface::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  uint32_t newClosingTime = (newValue->DurationMS & 0xFFFF) * 100;
  uint32_t newOpeningTime = ((newValue->DurationMS >> 16) & 0xFFFF) * 100;

  setOpenCloseTime(newClosingTime, newOpeningTime);

  int8_t task = newValue->value[0];
  int8_t tilt = newValue->value[1];
  SUPLA_LOG_DEBUG("RS[%d] new value from server: position/task %d, tilt %d",
      channel.getChannelNumber(), task, tilt);
  // TODO(klew): add tilt support
  switch (task) {
    case 0: {
      stop();
      break;
    }
    case 1: {
      moveDown();
      break;
    }
    case 2: {
      moveUp();
      break;
    }
    case 3: {  // down or stop
      if (inMove()) {
        stop();
      } else {
        moveDown();
      }
      break;
    }
    case 4: {  // up or stop
      if (inMove()) {
        stop();
      } else {
        moveUp();
      }
      break;
    }
    case 5: {  // sbs
      if (inMove()) {
        stop();
      } else if (lastDirectionWasOpen()) {
        moveDown();
      } else if (lastDirectionWasClose()) {
        moveUp();
      } else if (currentPosition < 50) {
        moveDown();
      } else {
        moveUp();
      }
      break;
    }

    default: {
      if (task >= 10 && task <= 110) {
        setTargetPosition(task - 10);
      }
      break;
    }
  }
  return -1;
}

void RollerShutterInterface::setOpenCloseTime(uint32_t newClosingTimeMs,
                                     uint32_t newOpeningTimeMs) {
  if (isTimeSettingAvailable()) {
    if (newClosingTimeMs != closingTimeMs ||
        newOpeningTimeMs != openingTimeMs) {
      closingTimeMs = newClosingTimeMs;
      openingTimeMs = newOpeningTimeMs;
      setCalibrationNeeded();
      SUPLA_LOG_DEBUG(
          "RS[%d] new time settings received. Opening time: %d ms; "
          "closing time: %d ms. Starting calibration...",
          channel.getChannelNumber(),
          openingTimeMs,
          closingTimeMs);
    }
  }
}

void RollerShutterInterface::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case CLOSE_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        close();
      }
      break;
    }

    case CLOSE: {
      close();
      break;
    }
    case OPEN_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        open();
      }
      break;
    }

    case OPEN: {
      open();
      break;
    }

    case COMFORT_DOWN_POSITION: {
      setTargetPosition(comfortDownValue);
      break;
    }

    case COMFORT_UP_POSITION: {
      setTargetPosition(comfortUpValue);
      break;
    }

    case STOP: {
      stop();
      break;
    }

    case STEP_BY_STEP: {
      if (inMove()) {
        stop();
      } else if (lastDirectionWasOpen()) {
        moveDown();
      } else if (lastDirectionWasClose()) {
        moveUp();
      } else if (currentPosition < 50) {
        moveDown();
      } else {
        moveUp();
      }
      break;
    }

    case MOVE_UP: {
      moveUp();
      break;
    }

    case MOVE_DOWN: {
      moveDown();
      break;
    }

    case MOVE_UP_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        moveUp();
      }
      break;
    }
    case INTERNAL_BUTTON_MOVE_UP_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        if (rsConfig.buttonsUpsideDown == 2) {
          moveDown();
        } else {
          moveUp();
        }
      }
      break;
    }
    case MOVE_DOWN_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        moveDown();
      }
      break;
    }
    case INTERNAL_BUTTON_MOVE_DOWN_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        if (rsConfig.buttonsUpsideDown == 2) {
          moveUp();
        } else {
          moveDown();
        }
      }
      break;
    }
  }
}

void RollerShutterInterface::close() {
  setTargetPosition(100);
}

void RollerShutterInterface::open() {
  setTargetPosition(0);
}

void RollerShutterInterface::moveDown() {
  setTargetPosition(MOVE_DOWN_POSITION);
}

void RollerShutterInterface::moveUp() {
  setTargetPosition(MOVE_UP_POSITION);
}

void RollerShutterInterface::stop() {
  setTargetPosition(STOP_POSITION);
}

void RollerShutterInterface::setCurrentPosition(int newPosition) {
  if (newPosition < 0) {
    newPosition = 0;
  } else if (newPosition > 100) {
    newPosition = 100;
  }
  currentPosition = newPosition;
  calibrate = false;
}

void RollerShutterInterface::setTargetPosition(int newPosition) {
  targetPosition = newPosition;
  newTargetPositionAvailable = true;

  if (targetPosition == MOVE_UP_POSITION) {
    lastDirection = Directions::UP_DIR;
  } else if (targetPosition == MOVE_DOWN_POSITION) {
    lastDirection = Directions::DOWN_DIR;
  } else if (targetPosition >= 0) {
    if (targetPosition < currentPosition) {
      lastDirection = Directions::UP_DIR;
    } else if (targetPosition > currentPosition) {
      lastDirection = Directions::DOWN_DIR;
    }
  }
}

bool RollerShutterInterface::lastDirectionWasOpen() const {
  return lastDirection == Directions::UP_DIR;
}

bool RollerShutterInterface::lastDirectionWasClose() const {
  return lastDirection == Directions::DOWN_DIR;
}

bool RollerShutterInterface::inMove() {
  return currentDirection != Directions::STOP_DIR;
}

void RollerShutterInterface::triggerCalibration() {
  setCalibrationNeeded();
  setTargetPosition(0);
}

void RollerShutterInterface::setCalibrationNeeded() {
  setCurrentPosition(UNKNOWN_POSITION);
  calibrate = true;
}

bool RollerShutterInterface::isCalibrationRequested() const {
  return calibrate && (!isTimeSettingAvailable() ||
                       (openingTimeMs != 0 && closingTimeMs != 0));
}

bool RollerShutterInterface::isCalibrated() const {
  if (isTimeSettingAvailable()) {
    return calibrate == false && openingTimeMs != 0 && closingTimeMs != 0;
  } else {
    return calibrate == false && currentPosition != UNKNOWN_POSITION;
  }
}

bool RollerShutterInterface::isCalibrationInProgress() const {
  return calibrationTime > 0;
}

void RollerShutterInterface::iterateAlways() {
  TDSC_RollerShutterValue value = {};
  value.position = currentPosition;
  if (isCalibrationInProgress()) {
    value.flags |= RS_VALUE_FLAG_CALIBRATION_IN_PROGRESS;
  }
  channel.setNewValue(value);
}

void RollerShutterInterface::configComfortUpValue(uint8_t position) {
  comfortUpValue = position;
  if (comfortUpValue > 100) {
    comfortUpValue = 100;
  }
}

void RollerShutterInterface::configComfortDownValue(uint8_t position) {
  comfortDownValue = position;
  if (comfortDownValue > 100) {
    comfortDownValue = 100;
  }
}

void RollerShutterInterface::onLoadState() {
  RollerShutterStateData data = RollerShutterStateData();
  if (Supla::Storage::ReadState((unsigned char *)&data, sizeof(data))) {
    closingTimeMs = data.closingTimeMs;
    openingTimeMs = data.openingTimeMs;
    currentPosition = data.currentPosition;
    if (currentPosition >= 0) {
      calibrate = false;
    }
    SUPLA_LOG_DEBUG(
        "RS[%d] settings restored from storage. Opening time: %d "
        "ms; closing time: %d ms. Position: %d",
        channel.getChannelNumber(),
        openingTimeMs,
        closingTimeMs,
        currentPosition);
  }
}

void RollerShutterInterface::onSaveState() {
  RollerShutterStateData data;
  data.closingTimeMs = closingTimeMs;
  data.openingTimeMs = openingTimeMs;
  data.currentPosition = currentPosition;

  Supla::Storage::WriteState((unsigned char *)&data, sizeof(data));
}

int RollerShutterInterface::getCurrentPosition() const {
  return currentPosition;
}

int RollerShutterInterface::getTargetPosition() const {
  return targetPosition;
}

int RollerShutterInterface::getCurrentDirection() const {
  return static_cast<int>(currentDirection);
}

uint32_t RollerShutterInterface::getClosingTimeMs() const {
  return closingTimeMs;
}

uint32_t RollerShutterInterface::getOpeningTimeMs() const {
  return openingTimeMs;
}

void RollerShutterInterface::attach(Supla::Control::Button *up,
    Supla::Control::Button *down) {
  upButton = up;
  downButton = down;
}

int RollerShutterInterface::handleCalcfgFromServer(
    TSD_DeviceCalCfgRequest *request) {
  if (request) {
    if (request->Command == SUPLA_CALCFG_CMD_RECALIBRATE) {
      if (!request->SuperUserAuthorized) {
        return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
      }
      SUPLA_LOG_INFO("RS[%d] - CALCFG recalibrate received",
          channel.getChannelNumber());
      triggerCalibration();
      return SUPLA_CALCFG_RESULT_DONE;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

uint8_t RollerShutterInterface::applyChannelConfig(TSD_ChannelConfig *result,
                                                   bool) {
  SUPLA_LOG_DEBUG(
      "RS[%d]:applyChannelConfig, func %d, configtype %d, configsize %d",
      getChannelNumber(),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  if (result->ConfigSize == 0) {
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  switch (result->Func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
      if (result->ConfigType == 0 &&
          result->ConfigSize == sizeof(TChannelConfig_RollerShutter)) {
        auto newConfig =
            reinterpret_cast<TChannelConfig_RollerShutter *>(result->Config);
        if (newConfig->OpeningTimeMS >= 0 && newConfig->ClosingTimeMS >= 0) {
          setOpenCloseTime(newConfig->ClosingTimeMS, newConfig->OpeningTimeMS);
        }
        if (!inMove()) {
          setTargetPosition(STOP_POSITION);
        }
        if (rsConfig.buttonsUpsideDown != 0) {
          if (newConfig->ButtonsUpsideDown > 0) {
            rsConfig.buttonsUpsideDown = newConfig->ButtonsUpsideDown;
          } else {
            triggerSetChannelConfig();
          }
        }
        if (rsConfig.motorUpsideDown != 0) {
          if (newConfig->MotorUpsideDown > 0) {
            rsConfig.motorUpsideDown = newConfig->MotorUpsideDown;
          } else {
            triggerSetChannelConfig();
          }
        }
        if (rsConfig.timeMargin != 0) {
          if (newConfig->TimeMargin != 0) {
            rsConfig.timeMargin = newConfig->TimeMargin;
          } else {
            triggerSetChannelConfig();
          }
        }
        rsConfig.visualizationType = newConfig->VisualizationType;
        if (rsConfig.buttonsUpsideDown > 2) {
          rsConfig.buttonsUpsideDown = 1;
        }
        if (rsConfig.motorUpsideDown > 2) {
          rsConfig.motorUpsideDown = 1;
        }
        if (rsConfig.timeMargin < -1) {
          rsConfig.timeMargin = -1;
        }
        if (rsConfig.timeMargin > 101) {
          rsConfig.timeMargin = 101;
        }
        saveConfig();
        printConfig();
        return SUPLA_CONFIG_RESULT_TRUE;
      }
      break;
    }

    default: {
      SUPLA_LOG_WARNING("RS[%d]: Ignoring unsupported channel function %d",
                        getChannelNumber(), result->Func);
      break;
    }
  }
  return SUPLA_CONFIG_RESULT_TRUE;
}

void RollerShutterInterface::onLoadConfig(SuplaDeviceClass *) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    loadFunctionFromConfig();

    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    RollerShutterConfig storedConfig = {};
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&storedConfig),
                     sizeof(RollerShutterConfig))) {
      if (rsConfig.motorUpsideDown != 0 && storedConfig.motorUpsideDown != 0) {
        rsConfig.motorUpsideDown = storedConfig.motorUpsideDown;
      }
      if (rsConfig.buttonsUpsideDown != 0 &&
          storedConfig.buttonsUpsideDown != 0) {
        rsConfig.buttonsUpsideDown = storedConfig.buttonsUpsideDown;
      }
      if (rsConfig.timeMargin != 0 && storedConfig.timeMargin != 0) {
        rsConfig.timeMargin = storedConfig.timeMargin;
      }
      rsConfig.visualizationType = storedConfig.visualizationType;
      printConfig();
    } else {
      SUPLA_LOG_DEBUG("RS[%d]: using default config", getChannelNumber());
    }
  }
}

void RollerShutterInterface::printConfig() const {
  SUPLA_LOG_INFO(
      "RS[%d]: rsConfig: motor: %s (%d), button: %s (%d), time "
      "margin: %d, visualization: %d",
      getChannelNumber(),
      rsConfig.motorUpsideDown == 2 ? "upside down" : "normal",
      rsConfig.motorUpsideDown,
      rsConfig.buttonsUpsideDown == 2 ? "upside down" : "normal",
      rsConfig.buttonsUpsideDown,
      rsConfig.timeMargin,
      rsConfig.visualizationType);
}

void RollerShutterInterface::saveConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    if (cfg->setBlob(key,
                     reinterpret_cast<char *>(&rsConfig),
                     sizeof(RollerShutterConfig))) {
      cfg->saveWithDelay(2000);
    }
  }
}

void RollerShutterInterface::setRsStorageSaveDelay(int delayMs) {
  rsStorageSaveDelay = delayMs;
}

void RollerShutterInterface::fillChannelConfig(void *channelConfig, int *size) {
  if (size) {
    *size = 0;
  } else {
    return;
  }
  if (channelConfig == nullptr) {
    return;
  }

  switch (channel.getDefaultFunction()) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
      SUPLA_LOG_DEBUG(
          "Relay[%d]: fill channel config for RS functions",
          channel.getChannelNumber());

      auto config = reinterpret_cast<TChannelConfig_RollerShutter *>(
          channelConfig);
      *size = sizeof(TChannelConfig_RollerShutter);
      config->ButtonsUpsideDown = rsConfig.buttonsUpsideDown;
      config->MotorUpsideDown = rsConfig.motorUpsideDown;
      config->TimeMargin = rsConfig.timeMargin;
      config->VisualizationType = rsConfig.visualizationType;
      config->OpeningTimeMS = openingTimeMs;
      config->ClosingTimeMS = closingTimeMs;
      break;
    }
    default:
      SUPLA_LOG_WARNING(
          "RS[%d]: fill channel config for unknown function %d",
          channel.getChannelNumber(),
          channel.getDefaultFunction());
      return;
  }
}

void RollerShutterInterface::setRsConfigMotorUpsideDownEnabled(bool enable) {
  if (enable) {
    if (rsConfig.motorUpsideDown == 0) {
      rsConfig.motorUpsideDown = 1;
    }
  } else {
    rsConfig.motorUpsideDown = 0;
  }
}

void RollerShutterInterface::setRsConfigButtonsUpsideDownEnabled(bool enable) {
  if (enable) {
    if (rsConfig.buttonsUpsideDown == 0) {
      rsConfig.buttonsUpsideDown = 1;
    }
  } else {
    rsConfig.buttonsUpsideDown = 0;
  }
}

void RollerShutterInterface::setRsConfigTimeMarginEnabled(bool enable) {
  if (enable) {
    if (rsConfig.timeMargin == 0) {
      rsConfig.timeMargin = -1;
    }
  } else {
    rsConfig.timeMargin = 0;
  }
}

uint32_t RollerShutterInterface::getTimeMarginValue(uint32_t fullTime) const {
  if (fullTime == 0) {
    return RS_DEFAULT_OPERATION_TIMEOUT_MS;
  }
  if (rsConfig.timeMargin <= 0) {
    // case for -1 (device specific) and 0 (not used)
    return fullTime * 0.3;
  }
  if (rsConfig.timeMargin == 1) {
    return 50;
  }
  uint32_t margin = fullTime * ((rsConfig.timeMargin - 1) / 100.0);
  if (margin < 50) {
    return 50;
  }
  return margin;
}

void RollerShutterInterface::fillSuplaChannelNewValue(
    TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }

  value->DurationMS = ((closingTimeMs / 100) & 0xFFFF) |
                      (((openingTimeMs / 100) & 0xFFFF) << 16);
}

uint8_t RollerShutterInterface::getMotorUpsideDown() const {
  return rsConfig.motorUpsideDown;
}

uint8_t RollerShutterInterface::getButtonsUpsideDown() const {
  return rsConfig.buttonsUpsideDown;
}

int8_t RollerShutterInterface::getTimeMargin() const {
  return rsConfig.timeMargin;
}

bool RollerShutterInterface::isAutoCalibrationSupported() const {
  return false;
  // TODO(klew): implement
}

void RollerShutterInterface::setRsConfigMotorUpsideDownValue(uint8_t value) {
  if (getMotorUpsideDown() != 0 && value > 0 && value < 3) {
    rsConfig.motorUpsideDown = value;
    saveConfig();
  }
}

void RollerShutterInterface::setRsConfigButtonsUpsideDownValue(uint8_t value) {
  if (getButtonsUpsideDown() != 0 && value > 0 && value < 3) {
    rsConfig.buttonsUpsideDown = value;
    saveConfig();
  }
}

void RollerShutterInterface::setRsConfigTimeMarginValue(int8_t value) {
  if (value >= -1 && value <= 100) {
    rsConfig.timeMargin = value;
    saveConfig();
  }
}

void RollerShutterInterface::purgeConfig() {
  Supla::ChannelElement::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    cfg->eraseKey(key);
  }
}

bool RollerShutterInterface::isTimeSettingAvailable() const {
  return (channel.getFlags() & SUPLA_CHANNEL_FLAG_TIME_SETTING_NOT_AVAILABLE) ==
         0;
}

