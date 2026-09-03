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

namespace Supla {
namespace Control {

HvacWeeklySchedule::HvacWeeklySchedule(HvacBase *owner) : owner_(owner) {
}

HvacWeeklySchedule::~HvacWeeklySchedule() {
}

bool HvacWeeklySchedule::canActivate() const {
  return isConfigured();
}

bool HvacWeeklySchedule::isConfigured() const {
  return nativeStorage_.configured;
}

bool HvacWeeklySchedule::supportsConfigType(uint8_t configType) const {
  if (configType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
    return true;
  }
  return configType == SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE && owner_ &&
         owner_->isAltWeeklySchedulePossible();
}

Supla::ApplyConfigResult HvacWeeklySchedule::applyChannelConfig(
    TSD_ChannelConfig *config, bool local) {
  if (config == nullptr || !supportsConfigType(config->ConfigType)) {
    return Supla::ApplyConfigResult::NotSupported;
  }
  const bool isAltWeeklySchedule =
      config->ConfigType == SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE;

  if (config->ConfigSize == 0) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d]: Missing weekly schedule on server. Sending local schedule",
        owner_->getChannelNumber());
    if (!ensureScheduleForUse(isAltWeeklySchedule)) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d]: No weekly schedule configured. Using SW defaults",
          owner_->getChannelNumber());
      return Supla::ApplyConfigResult::DataError;
    }
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  if (config->ConfigSize < sizeof(TChannelConfig_WeeklySchedule)) {
    SUPLA_LOG_WARNING("HVAC[%d]: Invalid weekly schedule",
                      owner_->getChannelNumber());
    return Supla::ApplyConfigResult::DataError;
  }

  auto *newSchedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config->Config);
  if (!isWeeklyScheduleValid(newSchedule, isAltWeeklySchedule)) {
    return Supla::ApplyConfigResult::DataError;
  }

  auto *schedule = getSchedule(isAltWeeklySchedule, false);
  bool scheduleAllocated = false;
  if (schedule == nullptr) {
    schedule = new TChannelConfig_WeeklySchedule();
    nativeStorage_.buffer.set(isAltWeeklySchedule, schedule);
    scheduleAllocated = true;
  }

  if (scheduleAllocated || !nativeStorage_.configured ||
      memcmp(schedule, newSchedule, sizeof(TChannelConfig_WeeklySchedule)) !=
          0) {
    memcpy(schedule, newSchedule, sizeof(TChannelConfig_WeeklySchedule));
    nativeStorage_.configured = true;
    saveWeeklyScheduleForType(isAltWeeklySchedule, local);
  }

  return Supla::ApplyConfigResult::Success;
}

void HvacWeeklySchedule::fillChannelConfig(void *channelConfig,
                                           int *size,
                                           uint8_t configType) {
  if (!supportsConfigType(configType)) {
    if (size) {
      *size = 0;
    }
    return;
  }
  fillChannelConfig(
      channelConfig,
      size,
      configType == SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE);
}

void HvacWeeklySchedule::purgeConfig() {
  if (owner_ == nullptr) {
    return;
  }
  nativeStorage_.erase(false, getStorageAccess(false));
  nativeStorage_.erase(true, getStorageAccess(true));
  nativeStorage_.reset();
  nativeStorage_.configured = true;
}

int HvacWeeklySchedule::getScheduleOwnerChannelNumber() const {
  return owner_ ? owner_->getChannelNumber() : -1;
}

const char *HvacWeeklySchedule::getScheduleOwnerLabel() const {
  return "HVAC";
}

const char *HvacWeeklySchedule::getScheduleLabel(
    bool isAltWeeklySchedule) const {
  return isAltWeeklySchedule ? "alt weekly schedule" : "weekly schedule";
}

const char *HvacWeeklySchedule::getScheduleStorageTag(
    bool isAltWeeklySchedule) const {
  return isAltWeeklySchedule ? Supla::ConfigTag::HvacAltWeeklyCfgTag
                             : Supla::ConfigTag::HvacWeeklyCfgTag;
}

void HvacWeeklySchedule::generateScheduleStorageKey(
    char *key, const char *storageTag) const {
  owner_->generateKey(key, storageTag);
}

bool HvacWeeklySchedule::validateNativeSchedule(
    const TChannelConfig_WeeklySchedule *schedule,
    bool isAltWeeklySchedule) const {
  return isWeeklyScheduleValid(schedule, isAltWeeklySchedule);
}

NativeWeeklyScheduleStorageAccess HvacWeeklySchedule::getStorageAccess(
    bool isAltWeeklySchedule) {
  NativeWeeklyScheduleStorageAccess access;
  access.channelNumber = getScheduleOwnerChannelNumber();
  access.deviceLabel = getScheduleOwnerLabel();
  access.scheduleLabel = getScheduleLabel(isAltWeeklySchedule);
  access.storageTag = getScheduleStorageTag(isAltWeeklySchedule);
  access.context = this;
  access.generateKey = generateScheduleStorageKeyCallback;
  access.validate = validateNativeScheduleCallback;
  return access;
}

void HvacWeeklySchedule::generateScheduleStorageKeyCallback(
    void *context, char *key, const char *storageTag) {
  static_cast<HvacWeeklySchedule *>(context)->generateScheduleStorageKey(
      key, storageTag);
}

bool HvacWeeklySchedule::validateNativeScheduleCallback(
    void *context,
    const TChannelConfig_WeeklySchedule *schedule,
    bool isAltWeeklySchedule) {
  return static_cast<HvacWeeklySchedule *>(context)->validateNativeSchedule(
      schedule, isAltWeeklySchedule);
}

bool HvacWeeklySchedule::isActive() const {
  return owner_ && owner_->isWeeklyScheduleEnabled();
}

bool HvacWeeklySchedule::switchToWeeklySchedule() {
  return turnOnWeeklySchedule();
}

void HvacWeeklySchedule::switchToManualMode() {
  if (owner_) {
    owner_->channel.setHvacFlagWeeklySchedule(false);
  }
}

void HvacWeeklySchedule::restoreWeeklyScheduleMode(bool enabled) {
  if (owner_) {
    owner_->channel.setHvacFlagWeeklySchedule(enabled);
  }
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
  if (!nativeStorage_.load(
          isAltWeeklySchedule, getStorageAccess(isAltWeeklySchedule))) {
    return false;
  }
  nativeStorage_.configured = true;
  nativeStorage_.cacheRuntime.touch(
      owner_->channel.isHvacFlagWeeklySchedule(), millis());
  return true;
}

bool HvacWeeklySchedule::ensureScheduleForUse(bool isAltWeeklySchedule) {
  if (ensureScheduleLoaded(isAltWeeklySchedule)) {
    return true;
  }

  initDefaultWeeklyScheduleForType(isAltWeeklySchedule, false);
  return nativeStorage_.buffer.get(isAltWeeklySchedule) != nullptr;
}

TChannelConfig_WeeklySchedule *HvacWeeklySchedule::getSchedule(
    bool isAltWeeklySchedule, bool loadIfMissing) {
  auto *schedule = nativeStorage_.buffer.get(isAltWeeklySchedule);
  if (schedule == nullptr && loadIfMissing) {
    if (!loadSchedule(isAltWeeklySchedule)) {
      return nullptr;
    }
    schedule = nativeStorage_.buffer.get(isAltWeeklySchedule);
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
  if (nativeStorage_.isPersisted(false)) {
    nativeStorage_.buffer.clear(false);
  }
  if (nativeStorage_.isPersisted(true)) {
    nativeStorage_.buffer.clear(true);
  }
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

  // HVAC has a built-in default schedule. Keep it logical until the schedule
  // is actually needed, so startup does not allocate either large buffer.
  nativeStorage_.reset();
  nativeStorage_.configured = true;
}

void HvacWeeklySchedule::processCacheRelease() {
  if (owner_ == nullptr) {
    return;
  }

  if (nativeStorage_.cacheRuntime.process(
          owner_->channel.isHvacFlagWeeklySchedule(), millis())) {
    unloadSchedulesIfPossible();
  }
}

void HvacWeeklySchedule::saveWeeklySchedule(bool requestResend) {
  if (owner_ == nullptr) {
    return;
  }

  if (nativeStorage_.buffer.get(false) != nullptr) {
    saveWeeklyScheduleForType(false, requestResend);
  }
  if (nativeStorage_.buffer.get(true) != nullptr) {
    saveWeeklyScheduleForType(true, requestResend);
  }
}

void HvacWeeklySchedule::saveWeeklyScheduleForType(bool isAltWeeklySchedule,
                                                   bool requestResend) {
  if (owner_ == nullptr) {
    return;
  }

  auto schedule = getSchedule(isAltWeeklySchedule, false);
  if (schedule == nullptr) {
    return;
  }

  nativeStorage_.save(
      isAltWeeklySchedule, getStorageAccess(isAltWeeklySchedule));

  if (requestResend && owner_->initDone) {
    owner_->requestWeeklyScheduleResend(isAltWeeklySchedule, true);
    owner_->syncWeeklyScheduleConfigTypes();
    owner_->persistChannelConfigChangeState();
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  cfg->saveWithDelay(5000);
  nativeStorage_.cacheRuntime.touch(
      owner_->channel.isHvacFlagWeeklySchedule(), millis());
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
    auto *self = const_cast<HvacWeeklySchedule *>(this);
    if (self->ensureScheduleForUse(false)) {
      schedule = self->getSchedule(false, false);
    }
  }
  return nativeStorage_.buffer.getProgramId(schedule, index);
}

int HvacWeeklySchedule::calculateIndex(enum DayOfWeek dayOfWeek,
                                       int hour,
                                       int quarter) const {
  return nativeStorage_.buffer.calculateIndex(dayOfWeek, hour, quarter);
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

  if (!ensureScheduleForUse(isAltWeeklySchedule)) {
    return false;
  }
  auto schedule = getSchedule(isAltWeeklySchedule, false);

  if (programId > 0 &&
      (schedule->Program[programId - 1].Mode == SUPLA_HVAC_MODE_NOT_SET ||
       schedule->Program[programId - 1].Mode > SUPLA_HVAC_MODE_DRY)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: invalid mode %d for programId %d",
                    owner_->getChannelNumber(),
                    schedule->Program[programId - 1].Mode,
                    programId);
    return false;
  }

  nativeStorage_.buffer.setWeeklySchedule(schedule, index, programId);

  if (owner_->initDone) {
    nativeStorage_.configured = true;
    saveWeeklyScheduleForType(isAltWeeklySchedule, true);
  }

  nativeStorage_.configured = true;
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

  if (!ensureScheduleForUse(isAltWeeklySchedule)) {
    return false;
  }
  auto schedule = getSchedule(isAltWeeklySchedule, false);

  schedule->Program[programId - 1].Mode = mode;
  schedule->Program[programId - 1].SetpointTemperatureHeat = tHeat;
  schedule->Program[programId - 1].SetpointTemperatureCool = tCool;

  if (owner_->initDone) {
    nativeStorage_.configured = true;
    saveWeeklyScheduleForType(isAltWeeklySchedule, true);
  }
  return true;
}

TWeeklyScheduleProgram HvacWeeklySchedule::getProgramById(
    int programId, bool isAltWeeklySchedule) const {
  if (programId < 1 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    return {};
  }

  auto *self = const_cast<HvacWeeklySchedule *>(this);
  if (!self->ensureScheduleForUse(isAltWeeklySchedule)) {
    return {};
  }
  auto schedule = self->getSchedule(isAltWeeklySchedule, false);

  return nativeStorage_.buffer.getProgramById(schedule, programId);
}

void HvacWeeklySchedule::fillChannelConfig(void *channelConfig,
                                           int *size,
                                           bool isAltWeeklySchedule) {
  if (size) {
    *size = 0;
  }
  if (channelConfig == nullptr || size == nullptr) {
    return;
  }

  if (!ensureScheduleForUse(isAltWeeklySchedule)) {
    return;
  }
  auto schedule = getSchedule(isAltWeeklySchedule, false);

  memcpy(channelConfig, schedule, sizeof(TChannelConfig_WeeklySchedule));
  *size = sizeof(TChannelConfig_WeeklySchedule);
}

TWeeklyScheduleProgram HvacWeeklySchedule::getProgramAt(
    int quarterIndex) const {
  bool useAlt = owner_->getChannel()->getDefaultFunction() ==
                    SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
                owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL;
  auto *self = const_cast<HvacWeeklySchedule *>(this);
  if (!self->ensureScheduleForUse(useAlt)) {
    return nativeStorage_.buffer.getProgramAt(nullptr, quarterIndex);
  }
  auto schedule = self->getSchedule(useAlt, false);
  return nativeStorage_.buffer.getProgramAt(schedule, quarterIndex);
}

int HvacWeeklySchedule::getCurrentQuarter() const {
  return nativeStorage_.buffer.getCurrentQuarter();
}

TWeeklyScheduleProgram HvacWeeklySchedule::getCurrentProgram() const {
  bool useAlt = owner_->getChannel()->getDefaultFunction() ==
                    SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
                owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL;
  auto *self = const_cast<HvacWeeklySchedule *>(this);
  if (!self->ensureScheduleForUse(useAlt)) {
    return nativeStorage_.buffer.getCurrentProgram(nullptr);
  }
  auto schedule = self->getSchedule(useAlt, false);
  return nativeStorage_.buffer.getCurrentProgram(schedule);
}

int HvacWeeklySchedule::getCurrentProgramId() const {
  bool useAlt = owner_->getChannel()->getDefaultFunction() ==
                    SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
                owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL;
  auto *self = const_cast<HvacWeeklySchedule *>(this);
  if (!self->ensureScheduleForUse(useAlt)) {
    return 1;
  }
  auto schedule = self->getSchedule(useAlt, false);
  return nativeStorage_.buffer.getCurrentProgramId(schedule);
}

bool HvacWeeklySchedule::turnOnWeeklySchedule() {
  bool useAlt = owner_->getChannel()->getDefaultFunction() ==
                    SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
                owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL;
  if (!ensureScheduleForUse(useAlt)) {
    return false;
  }
  nativeStorage_.cacheRuntime.touch(true, millis());
  return policy_.turnOnWeeklySchedule(*this);
}

bool HvacWeeklySchedule::processWeeklySchedule() {
  bool useAlt = owner_->getChannel()->getDefaultFunction() ==
                    SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
                owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL;
  if (!ensureScheduleForUse(useAlt)) {
    return false;
  }
  nativeStorage_.cacheRuntime.touch(true, millis());
  return policy_.processWeeklySchedule(*this);
}

void HvacWeeklySchedule::initDefaultWeeklySchedule(bool requestResend) {
  nativeStorage_.buffer.clearAll();
  initDefaultWeeklyScheduleForType(false, requestResend);
  if (owner_ != nullptr && owner_->getChannel()->getDefaultFunction() ==
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    initDefaultWeeklyScheduleForType(true, requestResend);
  }
}

void HvacWeeklySchedule::initDefaultWeeklyScheduleForType(
    bool isAltWeeklySchedule, bool requestResend) {
  policy_.initDefaultWeeklySchedule(
      *this, isAltWeeklySchedule, requestResend);
}

}  // namespace Control
}  // namespace Supla
