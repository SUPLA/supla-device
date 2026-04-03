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

#include "hvac_weekly_schedule.h"

#include <string.h>
#include <supla/clock/clock.h>
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

#include "hvac_base.h"
#include "weekly_schedule_common.h"

using Supla::Control::HvacBase;

#define USE_MAIN_WEEKLYSCHEDULE (false)
#define USE_ALT_WEEKLYSCHEDULE  (true)

namespace Supla {
namespace Control {

static constexpr _supla_int16_t kDefaultTempHeat = 2100;
static constexpr _supla_int16_t kDefaultTempCool = 2500;

HvacWeeklySchedule::HvacWeeklySchedule(HvacBase *owner) : owner_(owner) {
}

HvacWeeklySchedule::~HvacWeeklySchedule() {
}

const char *HvacWeeklySchedule::getStorageTag(bool isAltWeeklySchedule) {
  return isAltWeeklySchedule ? Supla::ConfigTag::HvacAltWeeklyCfgTag
                             : Supla::ConfigTag::HvacWeeklyCfgTag;
}

bool HvacWeeklySchedule::isConfigured() const {
  return isWeeklyScheduleConfigured_;
}

bool HvacWeeklySchedule::loadSchedule(bool isAltWeeklySchedule) {
  if (owner_ == nullptr || owner_->getChannelNumber() < 0) {
    return false;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return false;
  }

  auto *schedule = weeklyScheduleBuffer_.get(isAltWeeklySchedule);
  if (schedule == nullptr) {
    schedule = new TChannelConfig_WeeklySchedule();
    weeklyScheduleBuffer_.set(isAltWeeklySchedule, schedule);
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  owner_->generateKey(key, getStorageTag(isAltWeeklySchedule));
  if (!cfg->getBlob(key,
                    reinterpret_cast<char *>(schedule),
                    sizeof(TChannelConfig_WeeklySchedule))) {
    return false;
  }

  if (!isWeeklyScheduleValid(schedule, isAltWeeklySchedule)) {
    weeklyScheduleBuffer_.clear(isAltWeeklySchedule);
    return false;
  }

  isWeeklyScheduleConfigured_ = true;
  return true;
}

TChannelConfig_WeeklySchedule *HvacWeeklySchedule::getSchedule(
    bool isAltWeeklySchedule, bool loadIfMissing) {
  auto *schedule = weeklyScheduleBuffer_.get(isAltWeeklySchedule);
  if (schedule == nullptr && loadIfMissing) {
    if (!loadSchedule(isAltWeeklySchedule)) {
      return nullptr;
    }
    schedule = weeklyScheduleBuffer_.get(isAltWeeklySchedule);
  }
  return schedule;
}

const TChannelConfig_WeeklySchedule *HvacWeeklySchedule::getSchedule(
    bool isAltWeeklySchedule, bool loadIfMissing) const {
  return const_cast<HvacWeeklySchedule *>(this)->getSchedule(
      isAltWeeklySchedule, loadIfMissing);
}

void HvacWeeklySchedule::unloadSchedulesIfPossible() {
  if (owner_ == nullptr || owner_->channel.isHvacFlagWeeklySchedule()) {
    return;
  }

  weeklyScheduleBuffer_.clearAll();
}

void HvacWeeklySchedule::markWeeklyScheduleReceived(bool isAltWeeklySchedule) {
  if (isAltWeeklySchedule) {
    altWeeklyScheduleReceived_ = true;
  } else {
    weeklyScheduleReceived_ = true;
  }
}

void HvacWeeklySchedule::markWeeklyScheduleChangedOffline() {
  weeklyScheduleChangedOffline_ = 1;
}

bool HvacWeeklySchedule::isWeeklyScheduleChangedOffline() const {
  return weeklyScheduleChangedOffline_ == 1;
}

void HvacWeeklySchedule::onLoadConfig() {
  if (owner_ == nullptr) {
    return;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    SUPLA_LOG_ERROR("HVAC[%d]: can't work without config storage",
                    owner_->getChannelNumber());
    return;
  }

  isWeeklyScheduleConfigured_ = false;
  weeklyScheduleReceived_ = false;
  altWeeklyScheduleReceived_ = false;

  loadSchedule(false);
  if (owner_->getChannel()->getDefaultFunction() ==
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    loadSchedule(true);
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  uint8_t flag = 0;
  owner_->generateKey(key, Supla::ConfigTag::WeeklyScheduleChangedFlagTag);
  cfg->getUInt8(key, &flag);
  SUPLA_LOG_INFO("HVAC[%d]: weekly schedule config changed offline flag %d",
                 owner_->getChannelNumber(),
                 flag);
  weeklyScheduleChangedOffline_ = flag ? 1 : 0;
}

bool HvacWeeklySchedule::iterateConfigExchange() {
  if (owner_ == nullptr) {
    return true;
  }

  if (!owner_->configFinishedReceived || !owner_->serverChannelFunctionValid) {
    return true;
  }

  if (owner_->lastConfigChangeTimestampMs &&
      millis() - owner_->lastConfigChangeTimestampMs < 5000) {
    return true;
  }
  owner_->lastConfigChangeTimestampMs = 0;

  if (owner_->channelConfigChangedOffline == 1) {
    bool sent = false;
    for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
         proto = proto->next()) {
      owner_->config.ParameterFlags = owner_->parameterFlags;
      if (proto->setChannelConfig(owner_->getChannelNumber(),
                                  owner_->channel.getDefaultFunction(),
                                  reinterpret_cast<void *>(&owner_->config),
                                  sizeof(TChannelConfig_HVAC),
                                  SUPLA_CONFIG_TYPE_DEFAULT)) {
        SUPLA_LOG_INFO("HVAC[%d]: channel config send",
                       owner_->getChannelNumber());
        owner_->channelConfigChangedOffline = 2;
        sent = true;
      }
    }
    if (sent) {
      return false;
    }
  }

  if (owner_->channelConfigChangedOffline == 0 &&
      weeklyScheduleChangedOffline_ == 1) {
    bool sent = false;
    for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
         proto = proto->next()) {
      auto schedule = getSchedule(false, true);
      if (schedule == nullptr) {
        return true;
      }
      if (proto->setChannelConfig(owner_->getChannelNumber(),
                                  owner_->channel.getDefaultFunction(),
                                  reinterpret_cast<void *>(schedule),
                                  sizeof(TChannelConfig_WeeklySchedule),
                                  SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE)) {
        SUPLA_LOG_INFO("HVAC[%d]: weekly schedule send",
                       owner_->getChannelNumber());
        weeklyScheduleChangedOffline_ = 2;
        if (owner_->isAltWeeklySchedulePossible()) {
          auto alt = getSchedule(true, true);
          if (alt != nullptr &&
              proto->setChannelConfig(owner_->getChannelNumber(),
                                      owner_->channel.getDefaultFunction(),
                                      reinterpret_cast<void *>(alt),
                                      sizeof(TChannelConfig_WeeklySchedule),
                                      SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE)) {
            SUPLA_LOG_INFO("HVAC[%d]: alt weekly schedule send",
                           owner_->getChannelNumber());
          }
        }
        sent = true;
      }
    }
    if (sent) {
      return false;
    }
  }

  return true;
}

void HvacWeeklySchedule::releaseCacheIfPossible() {
  unloadSchedulesIfPossible();
}

void HvacWeeklySchedule::onRegistered() {
  if (weeklyScheduleChangedOffline_) {
    weeklyScheduleChangedOffline_ = 1;
  }
  weeklyScheduleReceived_ = false;
  altWeeklyScheduleReceived_ = false;
}

void HvacWeeklySchedule::handleChannelConfigFinished() {
  if (!weeklyScheduleReceived_) {
    weeklyScheduleChangedOffline_ = 1;
  }
  if (owner_->isAltWeeklySchedulePossible() && !altWeeklyScheduleReceived_) {
    weeklyScheduleChangedOffline_ = 1;
  }
}

uint8_t HvacWeeklySchedule::handleWeeklySchedule(
    TSD_ChannelConfig *newWeeklySchedule,
    bool isAltWeeklySchedule,
    bool local) {
  SUPLA_LOG_DEBUG("HVAC[%d]: Handling weekly schedule",
                  owner_->getChannelNumber());
  if (weeklyScheduleChangedOffline_) {
    SUPLA_LOG_INFO("HVAC[%d]: Ignoring%s weekly schedule",
                   owner_->getChannelNumber(),
                   isAltWeeklySchedule ? " alt" : "");
    markWeeklyScheduleReceived(isAltWeeklySchedule);
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  if (newWeeklySchedule == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  if (newWeeklySchedule->ConfigSize == 0) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d]: Missing weekly schedule on server. Sending local schedule",
        owner_->getChannelNumber());
    if (!isConfigured()) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d]: No weekly schedule configured. Using SW "
          "defaults",
          owner_->getChannelNumber());
      initDefaultWeeklySchedule();
    }
    markWeeklyScheduleChangedOffline();
    markWeeklyScheduleReceived(isAltWeeklySchedule);
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  if (newWeeklySchedule->ConfigSize < sizeof(TChannelConfig_WeeklySchedule)) {
    SUPLA_LOG_WARNING("HVAC[%d]: Invalid weekly schedule",
                      owner_->getChannelNumber());
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  auto newSchedule = reinterpret_cast<TChannelConfig_WeeklySchedule *>(
      newWeeklySchedule->Config);
  if (!isWeeklyScheduleValid(newSchedule, isAltWeeklySchedule)) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  auto schedule = getSchedule(isAltWeeklySchedule, true);
  if (schedule == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  markWeeklyScheduleReceived(isAltWeeklySchedule);
  if (!isConfigured() ||
      memcmp(schedule, newSchedule, sizeof(TChannelConfig_WeeklySchedule)) !=
          0) {
    memcpy(schedule, newSchedule, sizeof(TChannelConfig_WeeklySchedule));
    isWeeklyScheduleConfigured_ = true;
    if (!local) {
      weeklyScheduleChangedOffline_ = 0;
    }
    saveWeeklySchedule();
  }

  return SUPLA_CONFIG_RESULT_TRUE;
}

void HvacWeeklySchedule::handleSetChannelConfigResult(
    TSDS_SetChannelConfigResult *result) {
  if (result == nullptr) {
    return;
  }

  bool success = (result->Result == SUPLA_CONFIG_RESULT_TRUE);
  (void)(success);

  switch (result->ConfigType) {
    case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE:
    case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE: {
      SUPLA_LOG_INFO("HVAC[%d]: set weekly schedule config %s (%d)",
                     owner_->getChannelNumber(),
                     success ? "succeeded" : "failed",
                     result->Result);
      clearWeeklyScheduleChangedFlag();
      break;
    }
    default:
      break;
  }
}

void HvacWeeklySchedule::saveWeeklySchedule() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg || owner_ == nullptr) {
    return;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  auto schedule = getSchedule(false, true);
  if (schedule) {
    owner_->generateKey(key, Supla::ConfigTag::HvacWeeklyCfgTag);
    if (cfg->setBlob(key,
                     reinterpret_cast<char *>(schedule),
                     sizeof(TChannelConfig_WeeklySchedule))) {
      SUPLA_LOG_INFO("HVAC[%d]: weekly schedule saved successfully",
                     owner_->getChannelNumber());
    } else {
      SUPLA_LOG_WARNING("HVAC[%d]: failed to save weekly schedule",
                        owner_->getChannelNumber());
    }
  }

  if (owner_->getChannel()->getDefaultFunction() ==
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    auto alt = getSchedule(true, true);
    if (alt) {
      owner_->generateKey(key, Supla::ConfigTag::HvacAltWeeklyCfgTag);
      if (cfg->setBlob(key,
                       reinterpret_cast<char *>(alt),
                       sizeof(TChannelConfig_WeeklySchedule))) {
        SUPLA_LOG_INFO("HVAC[%d]: alt weekly schedule saved successfully",
                       owner_->getChannelNumber());
      } else {
        SUPLA_LOG_WARNING("HVAC[%d]: failed to save alt weekly schedule",
                          owner_->getChannelNumber());
      }
    }
  }

  owner_->generateKey(key, Supla::ConfigTag::WeeklyScheduleChangedFlagTag);
  cfg->setUInt8(key, weeklyScheduleChangedOffline_ ? 1 : 0);
  cfg->saveWithDelay(5000);
}

void HvacWeeklySchedule::clearWeeklyScheduleChangedFlag() {
  if (weeklyScheduleChangedOffline_) {
    weeklyScheduleChangedOffline_ = 0;
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg && owner_) {
      char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
      owner_->generateKey(key, Supla::ConfigTag::WeeklyScheduleChangedFlagTag);
      cfg->setUInt8(key, 0);
      cfg->saveWithDelay(1000);
    }
  }
}

bool HvacWeeklySchedule::isWeeklyScheduleValid(
    TChannelConfig_WeeklySchedule *newSchedule,
    bool isAltWeeklySchedule) const {
  bool programIsUsed[SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE] = {};

  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE; i++) {
    HvacBase::debugPrintProgram(&(newSchedule->Program[i]), i);
    if (!isProgramValid(newSchedule->Program[i], isAltWeeklySchedule)) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: weekly schedule validation failed: invalid program %d",
          owner_->getChannelNumber(),
          i);
      return false;
    }
    if (newSchedule->Program[i].Mode != SUPLA_HVAC_MODE_NOT_SET) {
      programIsUsed[i] = true;
    }
  }

  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; i++) {
    int programId = getWeeklyScheduleProgramId(newSchedule, i);
    if (programId != 0 && !programIsUsed[programId - 1]) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: weekly schedule validation failed: not configured program "
          "used in schedule %d",
          owner_->getChannelNumber(),
          i);
      return false;
    }
  }

  return true;
}

int HvacWeeklySchedule::getWeeklyScheduleProgramId(
    const TChannelConfig_WeeklySchedule *schedule, int index) const {
  if (schedule == nullptr) {
    schedule = getSchedule(false, true);
  }
  return weeklyScheduleBuffer_.getProgramId(schedule, index);
}

int HvacWeeklySchedule::calculateIndex(enum DayOfWeek dayOfWeek,
                                       int hour,
                                       int quarter) const {
  return weeklyScheduleBuffer_.calculateIndex(dayOfWeek, hour, quarter);
}

bool HvacWeeklySchedule::isProgramValid(const TWeeklyScheduleProgram &program,
                                        bool isAltWeeklySchedule) const {
  if (program.Mode == SUPLA_HVAC_MODE_NOT_SET) {
    return true;
  }

  if (program.Mode != SUPLA_HVAC_MODE_COOL &&
      program.Mode != SUPLA_HVAC_MODE_HEAT &&
      program.Mode != SUPLA_HVAC_MODE_HEAT_COOL) {
    return false;
  }

  auto channelFunction = owner_->getChannel()->getDefaultFunction();
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (program.Mode == SUPLA_HVAC_MODE_HEAT) {
      if (isAltWeeklySchedule) {
        return false;
      }
      if (!owner_->isHeatingAndCoolingSupported()) {
        return false;
      }
    } else if (program.Mode == SUPLA_HVAC_MODE_COOL) {
      if (!isAltWeeklySchedule) {
        return false;
      }
      if (!owner_->isHeatingAndCoolingSupported()) {
        return false;
      }
    } else if (program.Mode != SUPLA_HVAC_MODE_NOT_SET &&
               program.Mode != SUPLA_HVAC_MODE_OFF) {
      return false;
    }
  } else if (!owner_->isModeSupported(program.Mode)) {
    return false;
  }

  switch (program.Mode) {
    case SUPLA_HVAC_MODE_HEAT: {
      return owner_->isTemperatureInMainConstrain(
          program.SetpointTemperatureHeat);
    }
    case SUPLA_HVAC_MODE_COOL: {
      return owner_->isTemperatureInMainConstrain(
          program.SetpointTemperatureCool);
    }
    case SUPLA_HVAC_MODE_HEAT_COOL: {
      return owner_->isTemperatureInHeatCoolConstrain(
          program.SetpointTemperatureHeat, program.SetpointTemperatureCool);
    }
    default: {
      return false;
    }
  }
}

bool HvacWeeklySchedule::setWeeklySchedule(int index,
                                           int programId,
                                           bool isAltWeeklySchedule) {
  if (index < 0 || index >= SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d]: invalid index %d", owner_->getChannelNumber(), index);
    return false;
  }
  if (programId < 0 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    SUPLA_LOG_DEBUG("HVAC[%d]: invalid programId %d",
                    owner_->getChannelNumber(),
                    programId);
    return false;
  }

  auto schedule = getSchedule(isAltWeeklySchedule, true);
  if (schedule == nullptr) {
    return false;
  }

  if (programId > 0 &&
      (schedule->Program[programId - 1].Mode == SUPLA_HVAC_MODE_NOT_SET ||
       schedule->Program[programId - 1].Mode > SUPLA_HVAC_MODE_DRY)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: invalid mode %d for programId %d",
                    owner_->getChannelNumber(),
                    schedule->Program[programId - 1].Mode,
                    programId);
    return false;
  }

  weeklyScheduleBuffer_.setWeeklySchedule(schedule, index, programId);

  if (owner_->initDone) {
    weeklyScheduleChangedOffline_ = 1;
    isWeeklyScheduleConfigured_ = true;
    saveWeeklySchedule();
  }

  isWeeklyScheduleConfigured_ = true;
  return true;
}

bool HvacWeeklySchedule::setWeeklySchedule(enum DayOfWeek dayOfWeek,
                                           int hour,
                                           int quarter,
                                           int programId,
                                           bool isAltWeeklySchedule) {
  return setWeeklySchedule(
      calculateIndex(dayOfWeek, hour, quarter), programId, isAltWeeklySchedule);
}

bool HvacWeeklySchedule::setProgram(int programId,
                                    unsigned char mode,
                                    _supla_int16_t tHeat,
                                    _supla_int16_t tCool,
                                    bool isAltWeeklySchedule) {
  SUPLA_LOG_DEBUG("HVAC[%d]: set %s program(%d, %d, %d, %d)",
                  owner_->getChannelNumber(),
                  isAltWeeklySchedule ? "Alt" : "Main",
                  programId,
                  mode,
                  tHeat,
                  tCool);

  TWeeklyScheduleProgram program = {mode, {tHeat}, {tCool}};
  if (!isProgramValid(program, isAltWeeklySchedule)) {
    return false;
  }

  auto schedule = getSchedule(isAltWeeklySchedule, true);
  if (schedule == nullptr) {
    return false;
  }

  schedule->Program[programId - 1].Mode = mode;
  schedule->Program[programId - 1].SetpointTemperatureHeat = tHeat;
  schedule->Program[programId - 1].SetpointTemperatureCool = tCool;

  if (owner_->initDone) {
    weeklyScheduleChangedOffline_ = 1;
    isWeeklyScheduleConfigured_ = true;
    saveWeeklySchedule();
  }
  return true;
}

TWeeklyScheduleProgram HvacWeeklySchedule::getProgramById(
    int programId, bool isAltWeeklySchedule) const {
  if (programId < 1 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    return {};
  }

  auto schedule = getSchedule(isAltWeeklySchedule, true);
  if (schedule == nullptr) {
    return {};
  }

  return weeklyScheduleBuffer_.getProgramById(schedule, programId);
}

TWeeklyScheduleProgram HvacWeeklySchedule::getProgramAt(
    int quarterIndex) const {
  auto schedule = getSchedule(false, true);
  if (owner_->getChannel()->getDefaultFunction() ==
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL) {
      schedule = getSchedule(true, true);
    }
  }
  return weeklyScheduleBuffer_.getProgramAt(schedule, quarterIndex);
}

int HvacWeeklySchedule::getCurrentQuarter() const {
  return weeklyScheduleBuffer_.getCurrentQuarter();
}

TWeeklyScheduleProgram HvacWeeklySchedule::getCurrentProgram() const {
  auto schedule = getSchedule(false, true);
  if (owner_->getChannel()->getDefaultFunction() ==
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL) {
      schedule = getSchedule(true, true);
    }
  }
  return weeklyScheduleBuffer_.getCurrentProgram(schedule);
}

int HvacWeeklySchedule::getCurrentProgramId() const {
  auto schedule = getSchedule(false, true);
  if (owner_->getChannel()->getDefaultFunction() ==
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL) {
      schedule = getSchedule(true, true);
    }
  }

  return weeklyScheduleBuffer_.getCurrentProgramId(schedule);
}

bool HvacWeeklySchedule::turnOnWeeklySchedule() {
  if (!isConfigured()) {
    return false;
  }

  owner_->channel.setHvacFlagWeeklySchedule(true);
  return processWeeklySchedule();
}

bool HvacWeeklySchedule::processWeeklySchedule() {
  if (!owner_->channel.isHvacFlagWeeklySchedule()) {
    SUPLA_LOG_WARNING(
        "HVAC[%d]: processs weekly schedule failed - it is not "
        "enabled",
        owner_->getChannelNumber());
    return false;
  }

  if (!Supla::Clock::IsReady()) {
    if (owner_->startupDelay) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d]: Weekly schedule enabled, clock not ready -> "
          "startup delay...",
          owner_->getChannelNumber());
      return false;
    }

    if (!owner_->channel.isHvacFlagClockError()) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: processs weekly schedule failed - clock is "
          "not ready",
          owner_->getChannelNumber());
    }
    owner_->channel.setHvacFlagClockError(true);
  } else {
    owner_->channel.setHvacFlagClockError(false);
  }

  TWeeklyScheduleProgram program = getCurrentProgram();
  if (program.Mode == SUPLA_HVAC_MODE_NOT_SET) {
    SUPLA_LOG_INFO("HVAC[%d]: Invalid program mode. Disabling schedule.",
                   owner_->getChannelNumber());
    owner_->setTargetMode(SUPLA_HVAC_MODE_OFF, false);
    return false;
  }

  if (owner_->isWeelkySchedulManualOverrideMode()) {
    int currentProgramId = getCurrentProgramId();
    if (currentProgramId != owner_->lastProgramManualOverride) {
      SUPLA_LOG_DEBUG("HVAC[%d]: leaving manual override mode",
                      owner_->getChannelNumber());
      owner_->lastProgramManualOverride = -1;
    } else {
      if (owner_->getMode() == SUPLA_HVAC_MODE_OFF) {
        int mode = owner_->lastManualMode;
        if (mode == 0) {
          mode = owner_->getDefaultManualMode();
        }
        SUPLA_LOG_DEBUG("HVAC[%d]: Manual override mode %d",
                        owner_->getChannelNumber(),
                        mode);
        owner_->setTargetMode(mode, true);
      }
      if (!owner_->channel.isHvacFlagWeeklyScheduleTemporalOverride()) {
        SUPLA_LOG_DEBUG("HVAC[%d]: Manual override mode",
                        owner_->getChannelNumber());
      }
      owner_->channel.setHvacFlagWeeklyScheduleTemporalOverride(true);
      return true;
    }
  }
  owner_->channel.setHvacFlagWeeklyScheduleTemporalOverride(false);
  owner_->setTargetMode(program.Mode, true);
  int16_t tHeat = program.SetpointTemperatureHeat;
  int16_t tCool = program.SetpointTemperatureCool;
  if (program.Mode == SUPLA_HVAC_MODE_HEAT) {
    tCool = INT16_MIN;
  }
  if (program.Mode == SUPLA_HVAC_MODE_COOL) {
    tHeat = INT16_MIN;
  }
  owner_->setSetpointTemperaturesForCurrentMode(tHeat, tCool);
  return true;
}

void HvacWeeklySchedule::initDefaultWeeklySchedule() {
  isWeeklyScheduleConfigured_ = true;
  auto prevInitDone = owner_->initDone;
  if (owner_->initDone) {
    weeklyScheduleChangedOffline_ = 1;
    owner_->initDone = false;
  }

  weeklyScheduleBuffer_.clearAll();
  weeklyScheduleBuffer_.set(false, new TChannelConfig_WeeklySchedule());
  weeklyScheduleBuffer_.set(true, new TChannelConfig_WeeklySchedule());
  memset(weeklyScheduleBuffer_.get(false),
         0,
         sizeof(TChannelConfig_WeeklySchedule));
  memset(weeklyScheduleBuffer_.get(true),
         0,
         sizeof(TChannelConfig_WeeklySchedule));

  switch (owner_->getChannel()->getDefaultFunction()) {
    default: {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: no default weekly schedule defined for "
          "function %d",
          owner_->getChannelNumber(),
          owner_->getChannel()->getDefaultFunction());
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT: {
      setProgram(1, SUPLA_HVAC_MODE_HEAT, 1900, 0, USE_MAIN_WEEKLYSCHEDULE);
      setProgram(2, SUPLA_HVAC_MODE_HEAT, 2100, 0, USE_MAIN_WEEKLYSCHEDULE);
      setProgram(3, SUPLA_HVAC_MODE_HEAT, 3000, 0, USE_MAIN_WEEKLYSCHEDULE);
      setProgram(4, SUPLA_HVAC_MODE_HEAT, 1200, 0, USE_MAIN_WEEKLYSCHEDULE);

      setProgram(1, SUPLA_HVAC_MODE_COOL, 0, 2400, USE_ALT_WEEKLYSCHEDULE);
      setProgram(2, SUPLA_HVAC_MODE_COOL, 0, 2100, USE_ALT_WEEKLYSCHEDULE);
      setProgram(3, SUPLA_HVAC_MODE_COOL, 0, 1800, USE_ALT_WEEKLYSCHEDULE);
      setProgram(4, SUPLA_HVAC_MODE_COOL, 0, 2800, USE_ALT_WEEKLYSCHEDULE);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL: {
      setProgram(1, SUPLA_HVAC_MODE_HEAT_COOL, 1800, 2500);
      setProgram(2, SUPLA_HVAC_MODE_HEAT_COOL, 2100, 2400);
      setProgram(3, SUPLA_HVAC_MODE_HEAT, 2300, 0);
      setProgram(4, SUPLA_HVAC_MODE_COOL, 0, 2400);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL: {
      setProgram(1, SUPLA_HVAC_MODE_HEAT, -500, 0);
      setProgram(2, SUPLA_HVAC_MODE_HEAT, -200, 0);
      setProgram(3, SUPLA_HVAC_MODE_HEAT, -1000, 0);
      setProgram(4, SUPLA_HVAC_MODE_HEAT, -1500, 0);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      setProgram(1, SUPLA_HVAC_MODE_HEAT, 4000, 0);
      setProgram(2, SUPLA_HVAC_MODE_HEAT, 5000, 0);
      setProgram(3, SUPLA_HVAC_MODE_HEAT, 3000, 0);
      setProgram(4, SUPLA_HVAC_MODE_HEAT, 6000, 0);
      break;
    }
  }

  auto channelFunction = owner_->getChannel()->getDefaultFunction();
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 1;
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 2;
        } else {
          program = 1;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          setWeeklySchedule(static_cast<enum DayOfWeek>(dayOfAWeek),
                            hour,
                            quarter,
                            program,
                            USE_MAIN_WEEKLYSCHEDULE);
        }
      }
    }
  }
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 0;
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 1;
        } else {
          program = 0;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          setWeeklySchedule(static_cast<enum DayOfWeek>(dayOfAWeek),
                            hour,
                            quarter,
                            program,
                            USE_ALT_WEEKLYSCHEDULE);
        }
      }
    }
  }
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 1;
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 2;
        } else {
          program = 1;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          setWeeklySchedule(static_cast<enum DayOfWeek>(dayOfAWeek),
                            hour,
                            quarter,
                            program,
                            USE_MAIN_WEEKLYSCHEDULE);
        }
      }
    }
  }

  owner_->initDone = prevInitDone;
  saveWeeklySchedule();
}

}  // namespace Control
}  // namespace Supla
