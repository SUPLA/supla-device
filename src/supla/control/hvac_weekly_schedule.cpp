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
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

#include "hvac_base.h"
#include "weekly_schedule_common.h"
#include "weekly_schedule_storage.h"

namespace Supla {
namespace Control {

namespace {

constexpr uint32_t kWeeklyScheduleCacheReleaseDelayMs = 15000;

}  // namespace

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

bool HvacWeeklySchedule::ensureScheduleLoaded(bool isAltWeeklySchedule) {
  return getSchedule(isAltWeeklySchedule, true) != nullptr;
}

bool HvacWeeklySchedule::isProgramValid(const TWeeklyScheduleProgram &program,
                                        bool isAltWeeklySchedule) const {
  return policy_.isProgramValid(*owner_, program, isAltWeeklySchedule);
}

bool HvacWeeklySchedule::loadSchedule(bool isAltWeeklySchedule) {
  if (owner_ == nullptr || owner_->getChannelNumber() < 0) {
    return false;
  }

  auto *schedule = weeklyScheduleBuffer_.get(isAltWeeklySchedule);
  if (!WeeklyScheduleStorage::load(
          owner_->getChannelNumber(),
          "HVAC",
          "weekly schedule",
          getStorageTag(isAltWeeklySchedule),
          isAltWeeklySchedule,
          schedule,
          [this](char *key, const char *storageTag) {
            owner_->generateKey(key, storageTag);
          },
          [this, isAltWeeklySchedule](
              const TChannelConfig_WeeklySchedule *loadedSchedule) {
            return isWeeklyScheduleValid(loadedSchedule, isAltWeeklySchedule);
          })) {
    weeklyScheduleBuffer_.set(isAltWeeklySchedule, schedule);
    weeklyScheduleBuffer_.clear(isAltWeeklySchedule);
    return false;
  }
  weeklyScheduleBuffer_.set(isAltWeeklySchedule, schedule);
  isWeeklyScheduleConfigured_ = true;
  cacheRuntime_.touch(owner_->channel.isHvacFlagWeeklySchedule(), millis());
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

  SUPLA_LOG_DEBUG("HVAC[%d]: unloading weekly schedule cache",
                  owner_->getChannelNumber());
  weeklyScheduleBuffer_.clearAll();
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
  cacheRuntime_.reset();

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
  cacheRuntime_.touch(owner_->channel.isHvacFlagWeeklySchedule(), millis());
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

void HvacWeeklySchedule::processCacheRelease() {
  if (owner_ == nullptr) {
    return;
  }

  if (cacheRuntime_.process(owner_->channel.isHvacFlagWeeklySchedule(),
                            millis())) {
    unloadSchedulesIfPossible();
  }
}

void HvacWeeklySchedule::onRegistered() {
  if (weeklyScheduleChangedOffline_) {
    weeklyScheduleChangedOffline_ = 1;
  }
}

void HvacWeeklySchedule::handleChannelConfigFinished() {
  SUPLA_LOG_DEBUG("HVAC[%d]: channel config finished for weekly schedule",
                  owner_->getChannelNumber());
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

  if (!isConfigured() ||
      memcmp(schedule, newSchedule, sizeof(TChannelConfig_WeeklySchedule)) !=
          0) {
    memcpy(schedule, newSchedule, sizeof(TChannelConfig_WeeklySchedule));
    isWeeklyScheduleConfigured_ = true;
    if (!local) {
      weeklyScheduleChangedOffline_ = 0;
    }
    saveWeeklySchedule(local);
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

void HvacWeeklySchedule::saveWeeklySchedule(bool requestResend) {
  if (owner_ == nullptr) {
    return;
  }

  auto schedule = getSchedule(false, true);
  if (schedule != nullptr) {
    WeeklyScheduleStorage::save(owner_->getChannelNumber(),
                                "HVAC",
                                "weekly schedule",
                                Supla::ConfigTag::HvacWeeklyCfgTag,
                                schedule,
                                [this](char *key, const char *storageTag) {
                                  owner_->generateKey(key, storageTag);
                                });
    if (requestResend && owner_->initDone) {
      owner_->requestWeeklyScheduleResend(false);
    }
  }

  if (owner_->getChannel()->getDefaultFunction() ==
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    auto alt = getSchedule(true, true);
    if (alt != nullptr) {
      WeeklyScheduleStorage::save(owner_->getChannelNumber(),
                                  "HVAC",
                                  "alt weekly schedule",
                                  Supla::ConfigTag::HvacAltWeeklyCfgTag,
                                  alt,
                                  [this](char *key, const char *storageTag) {
                                    owner_->generateKey(key, storageTag);
                                  });
      if (requestResend && owner_->initDone) {
        owner_->requestWeeklyScheduleResend(true);
      }
    }
  }

  if (requestResend && owner_->initDone) {
    owner_->syncWeeklyScheduleConfigTypes();
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  owner_->generateKey(key, Supla::ConfigTag::WeeklyScheduleChangedFlagTag);
  cfg->setUInt8(key, weeklyScheduleChangedOffline_ ? 1 : 0);
  cfg->saveWithDelay(5000);
  cacheRuntime_.touch(owner_->channel.isHvacFlagWeeklySchedule(), millis());
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
    const TChannelConfig_WeeklySchedule *newSchedule,
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
    if (programId < 0 ||
        programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: weekly schedule validation failed: invalid program %d "
          "used in schedule %d",
          owner_->getChannelNumber(),
          programId,
          i);
      return false;
    }
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
    saveWeeklySchedule(true);
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
  if (!policy_.isProgramValid(*owner_, program, isAltWeeklySchedule)) {
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
    saveWeeklySchedule(true);
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

void HvacWeeklySchedule::fillChannelConfig(void *channelConfig,
                                           int *size,
                                           bool isAltWeeklySchedule) const {
  if (size) {
    *size = 0;
  }
  if (channelConfig == nullptr || size == nullptr) {
    return;
  }

  auto schedule = getSchedule(isAltWeeklySchedule, true);
  if (schedule == nullptr) {
    return;
  }

  memcpy(channelConfig, schedule, sizeof(TChannelConfig_WeeklySchedule));
  *size = sizeof(TChannelConfig_WeeklySchedule);
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
  if (!ensureScheduleLoaded(false)) {
    return false;
  }
  if (owner_->getChannel()->getDefaultFunction() ==
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
      owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL) {
    if (!ensureScheduleLoaded(true)) {
      return false;
    }
  }
  if (!isConfigured()) {
    initDefaultWeeklySchedule();
  }
  cacheRuntime_.touch(true, millis());
  return policy_.turnOnWeeklySchedule(*this);
}

bool HvacWeeklySchedule::processWeeklySchedule() {
  if (!ensureScheduleLoaded(false)) {
    return false;
  }
  if (owner_->getChannel()->getDefaultFunction() ==
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
      owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL) {
    if (!ensureScheduleLoaded(true)) {
      return false;
    }
  }
  if (!isConfigured()) {
    initDefaultWeeklySchedule();
  }
  cacheRuntime_.touch(true, millis());
  return policy_.processWeeklySchedule(*this);
}

void HvacWeeklySchedule::initDefaultWeeklySchedule() {
  policy_.initDefaultWeeklySchedule(*this);
}

}  // namespace Control
}  // namespace Supla
