// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_
#define SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_

#include <stdint.h>
#include <supla-common/proto.h>

#include "../element_with_channel_actions.h"
#include "weekly_schedule_component.h"
#include "weekly_schedule_storage.h"

namespace Supla {
namespace Control {

class Relay;

class RelayWeeklySchedule : public NativeWeeklyScheduleController {
 public:
  explicit RelayWeeklySchedule(Relay *owner);
  ~RelayWeeklySchedule();

  bool isConfigured() const;
  bool iterateAlways();

  bool canActivate() const override;
  bool switchToWeeklySchedule() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool isWeeklyScheduleEnabled() const;
  bool isManualActionAllowed(bool turnOn) const override;
  void onManualAction() override;

 private:
  bool requiresReadyClock() const override { return true; }
  void onWeeklyScheduleClockState(WeeklyScheduleClockState state) override;
  bool applyProgramAt(const WeeklyScheduleTimeSnapshot &time,
                      const TWeeklyScheduleProgram &program,
                      int programId, bool programChanged) override;
  void syncWeeklyScheduleMode(uint8_t programMode) override;
  void scheduleWeeklyScheduleStateSave() override;
  bool isWeeklyScheduleValid(
      const TChannelConfig_WeeklySchedule *newSchedule) const;
  bool isProgramValid(const TWeeklyScheduleProgram &program) const;
  bool applyWeeklyScheduleMode(uint8_t mode, bool programChanged) override;
  uint8_t getCurrentProgramMode() const;

  Supla::Element *getScheduleOwner() const override;
  const char *getDeviceLabel() const override;
  const char *getScheduleStorageTag(bool alt) const override;
  bool validateSchedule(const TChannelConfig_WeeklySchedule *schedule,
                        bool alt) const override;
  void fillDefaultSchedule(TChannelConfig_WeeklySchedule *schedule,
                           bool alt) override;
  void onNativeScheduleApplied(bool alt, bool local, bool changed) override;
  void resetRuntimeOverride();
  bool processProgramAt(const WeeklyScheduleTimeSnapshot &time,
                        const TWeeklyScheduleProgram &program,
                        int programId, bool programChanged, bool manualAction);

  Relay *owner_ = nullptr;
  int32_t occurrence_ = -1;
  int32_t lastTimingQuarter_ = -1;
  uint32_t phase_ = UINT32_MAX;
  bool timed_ = false;
  bool suppressed_ = false;
  bool pendingManualAction_ = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_
