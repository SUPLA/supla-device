// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_COMPONENT_H_
#define SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_COMPONENT_H_

#include <stdint.h>
#include <supla-common/proto.h>
#include <supla/apply_config_result.h>

namespace Supla {
namespace Control {

class WeeklyScheduleConfigHandler;

enum class WeeklyScheduleClockState : uint8_t {
  Ready,
  Waiting,
  TimedOut,
};

class WeeklyScheduleController {
 public:
  virtual ~WeeklyScheduleController() = default;
  virtual WeeklyScheduleConfigHandler *getConfigHandler() {
    return nullptr;
  }
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
  bool updateCurrentProgramId(int programId);
  void resetCurrentProgramId();

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
