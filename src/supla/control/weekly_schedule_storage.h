// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_
#define SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_

#include <supla-common/proto.h>

#include "weekly_schedule_buffer.h"
#include "weekly_schedule_cache_runtime.h"
#include "weekly_schedule_common.h"
#include "weekly_schedule_component.h"

namespace Supla {
class Element;

namespace Control {

class NativeWeeklyScheduleConfigHandler : public WeeklyScheduleConfigHandler,
                                          public WeeklyScheduleProgramSource {
 public:
  void onLoadConfig() override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *config,
                                              bool local) override;
  void fillChannelConfig(void *config,
                         int *size,
                         uint8_t configType) override;
  void purgeConfig() override;
  bool supportsConfigType(uint8_t configType) const override;

 protected:
  virtual Supla::Element *getScheduleOwner() const = 0;
  virtual const char *getDeviceLabel() const = 0;
  virtual const char *getScheduleStorageTag(bool alt) const = 0;
  virtual bool validateSchedule(
      const TChannelConfig_WeeklySchedule *schedule, bool alt) const = 0;
  virtual void fillDefaultSchedule(TChannelConfig_WeeklySchedule *schedule,
                                   bool alt) = 0;
  virtual bool supportsAltSchedule() const {
    return false;
  }
  virtual bool hasAltScheduleStorage() const {
    return false;
  }
  virtual bool hasPersistentDefaultSchedule(bool alt) const {
    (void)(alt);
    return false;
  }
  virtual void onNativeScheduleLoaded() {
  }
  virtual void onNativeScheduleLoadFailed(bool alt) {
    (void)(alt);
  }
  virtual void onNativeScheduleApplied(bool alt, bool local, bool changed) {
    (void)(alt);
    (void)(local);
    (void)(changed);
  }
  virtual void onNativeScheduleSaved(bool alt, bool notify) {
    (void)(alt);
    (void)(notify);
  }
  virtual void onNativeSchedulePurged() {
  }

  bool isConfigured() const;
  bool isConfigured(bool alt) const;
  bool isPersisted(bool alt) const;

  TChannelConfig_WeeklySchedule *getSchedule(bool alt,
                                             bool loadIfMissing = true);
  const TChannelConfig_WeeklySchedule *getSchedule(
      bool alt, bool loadIfMissing = true) const;
  TChannelConfig_WeeklySchedule *ensureSchedule(bool alt);
  bool ensureScheduleForUse(bool alt);
  bool updateSchedule(bool alt,
                      const TChannelConfig_WeeklySchedule &schedule);
  bool saveSchedule(bool alt, bool notify = false);
  void clearSchedule(bool alt);
  void clearSchedules();
  void unloadSchedule(bool alt);

  int calculateIndex(enum DayOfWeek dayOfWeek, int hour, int quarter) const;
  int getProgramId(const TChannelConfig_WeeklySchedule *schedule,
                   int index) const;
  bool setWeeklySchedule(TChannelConfig_WeeklySchedule *schedule,
                         int index,
                         int programId) const;
  TWeeklyScheduleProgram getProgramById(
      const TChannelConfig_WeeklySchedule *schedule, int programId) const;
  TWeeklyScheduleProgram getProgramAt(
      const TChannelConfig_WeeklySchedule *schedule, int quarterIndex) const;
  int getCurrentQuarter() const;
  bool resolveCurrentProgram(bool alt,
                             TWeeklyScheduleProgram *program,
                             int *programId);
  bool resolveCurrentProgram(bool alt,
                             TWeeklyScheduleProgram *program,
                             int *programId) const;
  bool resolveCurrentProgram(bool alt,
                             const WeeklyScheduleTimeSnapshot &time,
                             TWeeklyScheduleProgram *program,
                             int *programId);
  bool resolveProgram(const WeeklyScheduleTimeSnapshot &time,
                      bool alt,
                      TWeeklyScheduleProgram *program,
                      int *programId) override;
  bool resolveProgramTiming(const WeeklyScheduleTimeSnapshot &time,
                            bool alt, int programId, int32_t *occurrence,
                            uint32_t *elapsedSeconds) override;

  void touchCache(bool active, uint32_t nowMs);
  bool processCache(bool active, uint32_t nowMs);
  void resetCache();

 private:
  bool loadSchedule(bool alt);
  void eraseSchedule(bool alt);
  bool configTypeToAlt(uint8_t configType, bool *alt) const;
  const char *getScheduleLabel(bool alt) const;

  WeeklyScheduleBuffer buffer_;
  WeeklyScheduleCacheRuntime cacheRuntime_;
  bool configured_[2] = {false, false};
  bool persisted_[2] = {false, false};
  bool loadAttempted_[2] = {false, false};
};

class NativeWeeklyScheduleController
    : public WeeklyScheduleController,
      public NativeWeeklyScheduleConfigHandler {
 public:
  bool canActivate() const override;
  bool isActive() const override;
  bool switchToWeeklySchedule() override;
  void switchToManualMode() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool processWeeklySchedule() override;
  bool isWeeklyScheduleConfigured() const;

 protected:
  virtual void syncWeeklyScheduleMode(uint8_t mode) = 0;
  virtual void scheduleWeeklyScheduleStateSave() = 0;
  virtual bool applyWeeklyScheduleMode(uint8_t mode, bool programChanged);

  void onNativeScheduleLoaded() override;
  void onNativeScheduleLoadFailed(bool alt) override;
  void onNativeScheduleApplied(bool alt,
                               bool local,
                               bool changed) override;
  void onNativeScheduleSaved(bool alt, bool notify) override;
  void onNativeSchedulePurged() override;
  bool applyResolvedWeeklyScheduleProgram(
      const TWeeklyScheduleProgram &program,
      int programId,
      bool programChanged) override;

 private:
  bool enabled_ = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_
