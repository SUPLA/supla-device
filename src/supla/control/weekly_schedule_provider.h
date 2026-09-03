// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_PROVIDER_H_
#define SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_PROVIDER_H_

#include <stdint.h>
#include <supla-common/proto.h>

#include "../element_with_channel_actions.h"
#include "weekly_schedule_buffer.h"
#include "weekly_schedule_cache_runtime.h"

namespace Supla {
namespace Control {

enum class WeeklyScheduleProviderType : uint8_t {
  NativeHvac,
  NativeRelay,
  ExternalManaged,
};

class WeeklyScheduleProvider {
 public:
  virtual ~WeeklyScheduleProvider() = default;

  virtual WeeklyScheduleProviderType getProviderType() const = 0;
  virtual void onLoadConfig() = 0;
  virtual bool supportsConfigType(uint8_t configType) const = 0;
  virtual Supla::ApplyConfigResult applyChannelConfig(
      TSD_ChannelConfig *config, bool local) = 0;
  virtual void fillChannelConfig(void *config,
                                 int *size,
                                 uint8_t configType) = 0;
  virtual void purgeConfig() = 0;
  virtual void processCacheRelease() = 0;

  virtual bool isConfigured() const = 0;
  virtual bool isActive() const = 0;
  virtual bool switchToWeeklySchedule() = 0;
  virtual void switchToManualMode() = 0;
  virtual void restoreWeeklyScheduleMode(bool enabled) = 0;
  virtual bool processWeeklySchedule() = 0;
  virtual bool isManualActionAllowed(bool turnOn) const = 0;
  virtual bool getCurrentProgram(TWeeklyScheduleProgram *program,
                                 int *programId) const = 0;
};

class NativeWeeklyScheduleProvider : public WeeklyScheduleProvider {
 public:
  bool isConfigured() const override;

 protected:
  bool loadNativeSchedule(bool alt);
  bool saveNativeSchedule(bool alt);
  void eraseNativeSchedule(bool alt);
  TChannelConfig_WeeklySchedule *getNativeSchedule(bool alt,
                                                   bool loadIfMissing = true);
  const TChannelConfig_WeeklySchedule *getNativeSchedule(
      bool alt, bool loadIfMissing = true) const;
  bool isNativeSchedulePersisted(bool alt) const;
  void resetNativeScheduleLifecycle();

  virtual int getScheduleOwnerChannelNumber() const = 0;
  virtual const char *getScheduleOwnerLabel() const = 0;
  virtual const char *getScheduleLabel(bool alt) const = 0;
  virtual const char *getScheduleStorageTag(bool alt) const = 0;
  virtual void generateScheduleStorageKey(char *key,
                                          const char *storageTag) const = 0;
  virtual bool validateNativeSchedule(
      const TChannelConfig_WeeklySchedule *schedule, bool alt) const = 0;

  WeeklyScheduleBuffer weeklyScheduleBuffer_;
  bool isWeeklyScheduleConfigured_ = false;
  WeeklyScheduleCacheRuntime cacheRuntime_;

 private:
  bool schedulePersisted_[2] = {false, false};
};

class ExternalManagedWeeklyScheduleProvider : public WeeklyScheduleProvider {
 public:
  WeeklyScheduleProviderType getProviderType() const override;
  void onLoadConfig() override;
  bool supportsConfigType(uint8_t configType) const override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *config,
                                               bool local) override;
  void fillChannelConfig(void *config,
                         int *size,
                         uint8_t configType) override;
  void purgeConfig() override;
  void processCacheRelease() override;

  bool isConfigured() const override;
  bool isActive() const override;
  bool switchToWeeklySchedule() override;
  void switchToManualMode() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool processWeeklySchedule() override;
  bool isManualActionAllowed(bool turnOn) const override;
  bool getCurrentProgram(TWeeklyScheduleProgram *program,
                         int *programId) const override;

 private:
  bool active_ = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_PROVIDER_H_
