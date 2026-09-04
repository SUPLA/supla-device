// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_COMPONENT_H_
#define SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_COMPONENT_H_

#include <stdint.h>
#include <supla-common/proto.h>
#include <supla/apply_config_result.h>

#include "weekly_schedule_common.h"

namespace Supla {
namespace Control {

class WeeklyScheduleConfigHandler;

enum class WeeklyScheduleClockState : uint8_t {
  Ready,
  Waiting,
  TimedOut,
};

struct WeeklyScheduleTimeSnapshot {
  WeeklyScheduleClockState state = WeeklyScheduleClockState::TimedOut;
  enum DayOfWeek dayOfWeek = DayOfWeek_Sunday;
  uint8_t hour = 0;
  uint8_t quarter = 0;
};

class WeeklyScheduleController {
 public:
  virtual ~WeeklyScheduleController() = default;
  virtual bool canActivate() const = 0;
  virtual bool isActive() const = 0;
  virtual bool switchToWeeklySchedule() = 0;
  virtual void switchToManualMode() = 0;
  virtual void restoreWeeklyScheduleMode(bool enabled) = 0;
  virtual bool processWeeklySchedule() = 0;
  virtual bool isManualActionAllowed(bool turnOn) const {
    (void)(turnOn);
    return true;
  }
  virtual bool isExternallyManaged() const {
    return false;
  }

 protected:
  WeeklyScheduleClockState getClockState() const;
  WeeklyScheduleClockState getClockState(bool startupDelay) const;
  WeeklyScheduleTimeSnapshot getWeeklyScheduleTimeSnapshot(
      bool startupDelay) const;
  bool processCurrentProgram(bool startupDelay);
  bool updateCurrentProgramId(int programId);
  void resetCurrentProgramId();

  virtual void onWeeklyScheduleClockState(WeeklyScheduleClockState state) {
    (void)(state);
  }
  virtual bool resolveWeeklyScheduleProgram(
      const WeeklyScheduleTimeSnapshot &time,
      TWeeklyScheduleProgram *program,
      int *programId);
  virtual bool applyResolvedWeeklyScheduleProgram(
      const TWeeklyScheduleProgram &program,
      int programId,
      bool programChanged);

 private:
  int currentProgramId_ = -1;
};

class WeeklyScheduleConfigHandler {
 public:
  virtual ~WeeklyScheduleConfigHandler() = default;
  virtual void onLoadConfig() = 0;
  virtual bool supportsConfigType(uint8_t configType) const = 0;
  virtual Supla::ApplyConfigResult applyChannelConfig(
      TSD_ChannelConfig *config, bool local) = 0;
  virtual void fillChannelConfig(void *config,
                                 int *size,
                                 uint8_t configType) = 0;
  virtual void purgeConfig() = 0;
};

class ExternalManagedWeeklySchedule : public WeeklyScheduleController {
 public:
  bool canActivate() const override;
  bool isActive() const override;
  bool switchToWeeklySchedule() override;
  void switchToManualMode() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool processWeeklySchedule() override;
  bool isExternallyManaged() const override {
    return true;
  }

 private:
  bool active_ = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_COMPONENT_H_
