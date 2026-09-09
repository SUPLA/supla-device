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
  uint16_t secondOfQuarter = 0;
  // Local civil day number, used to distinguish recurring weekly occurrences.
  int32_t dayNumber = 0;
};

class WeeklyScheduleProgramSource {
 public:
  virtual ~WeeklyScheduleProgramSource() = default;
  virtual bool resolveProgram(const WeeklyScheduleTimeSnapshot &time,
                              bool alt,
                              TWeeklyScheduleProgram *program,
                              int *programId) = 0;
  virtual bool resolveProgramTiming(const WeeklyScheduleTimeSnapshot &time,
                                    bool alt,
                                    int programId,
                                    int32_t *occurrence,
                                    uint32_t *elapsedSeconds) {
    (void)(time);
    (void)(alt);
    (void)(programId);
    (void)(occurrence);
    (void)(elapsedSeconds);
    return false;
  }
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
  virtual void onManualAction() {}

 protected:
  WeeklyScheduleClockState getClockState() const;
  WeeklyScheduleClockState getClockState(bool startupDelay) const;
  WeeklyScheduleTimeSnapshot getWeeklyScheduleTimeSnapshot(
      bool startupDelay) const;
  bool processCurrentProgram(bool startupDelay);
  bool updateCurrentProgramId(int programId);
  void resetCurrentProgramId();
  // The source is not owned and has to remain valid while registered.
  void setWeeklyScheduleProgramSource(WeeklyScheduleProgramSource *source);

  virtual void onWeeklyScheduleClockState(WeeklyScheduleClockState state) {
    (void)(state);
  }
  virtual bool shouldUseAltWeeklySchedule() const {
    return false;
  }
  virtual bool requiresReadyClock() const { return false; }
  bool resolveProgramTiming(const WeeklyScheduleTimeSnapshot &time,
                            int programId,
                            int32_t *occurrence,
                            uint32_t *elapsedSeconds);
  virtual bool applyProgramAt(const WeeklyScheduleTimeSnapshot &time,
                              const TWeeklyScheduleProgram &program,
                              int programId,
                              bool programChanged);
  bool resolveWeeklyScheduleProgram(
      const WeeklyScheduleTimeSnapshot &time,
      TWeeklyScheduleProgram *program,
      int *programId);
  virtual bool applyResolvedWeeklyScheduleProgram(
      const TWeeklyScheduleProgram &program,
      int programId,
      bool programChanged);

 private:
  friend class WeeklyScheduleComponents;
  WeeklyScheduleProgramSource *programSource_ = nullptr;
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

class WeeklyScheduleComponents {
 public:
  WeeklyScheduleComponents() = default;
  ~WeeklyScheduleComponents();
  WeeklyScheduleComponents(const WeeklyScheduleComponents &) = delete;
  WeeklyScheduleComponents &operator=(const WeeklyScheduleComponents &) =
      delete;

  // Ownership of the controller is transferred only when true is returned.
  // The config handler and program source are not owned.
  bool set(WeeklyScheduleController *controller,
           WeeklyScheduleConfigHandler *configHandler = nullptr,
           WeeklyScheduleProgramSource *programSource = nullptr);
  void loadConfig();
  bool isAssigned() const;
  bool isStarted() const;
  WeeklyScheduleController *getController() const;
  WeeklyScheduleConfigHandler *getConfigHandler() const;
  WeeklyScheduleProgramSource *getProgramSource() const;

 private:
  enum class LifecycleState : uint8_t {
    Unassigned,
    Assigned,
    Started,
  };

  LifecycleState lifecycleState_ = LifecycleState::Unassigned;
  WeeklyScheduleController *controller_ = nullptr;
  WeeklyScheduleConfigHandler *configHandler_ = nullptr;
  WeeklyScheduleProgramSource *programSource_ = nullptr;
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
