// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CLOCK_CLOCK_H_
#define SRC_SUPLA_CLOCK_CLOCK_H_

#include <supla-common/proto.h>
#include <supla/element.h>
#include <time.h>
#include <supla/control/hvac_base.h>

namespace Supla {

const char AutomaticTimeSyncCfgTag[] = "timesync_auto";

class Clock : public Element {
 public:
  /**
  * Checks if Clock is set and ready.
  *
  * @return true if Clock is set and ready
  */
  static bool IsReady();

  /**
  * Returns year from Clock (if is set and ready).
  *
  * @return the year (1900..)
  */
  static int GetYear();

  /**
  * Returns month from Clock (if is set and ready).
  *
  * @return the month (1..12)
  */
  static int GetMonth();

  /**
  * Returns day from Clock (if is set and ready).
  *
  * @return the day (1..31)
  */
  static int GetDay();

  /**
  * Returns day of week from Clock (if is set and ready).
  * Example:
  * 1 - Sunday, 2 - Monday
  *
  * @return the day of week (1..2)
  */
  static int GetDayOfWeek();

  /**
  * Returns day of week from Clock to HVAC (if is set and ready).
  * Example:
  * 1 - Sunday, 2 - Monday
  *
  * @return the day of week (1..2)
  */
  static enum DayOfWeek GetHvacDayOfWeek();

  /**
  * Returns hour from Clock (if is set and ready).
  *
  * @return the hour (0..23)
  */
  static int GetHour();

  /**
  * Returns quarter from Clock (if is set and ready).
  * Example:
  * 0 - (0..14 min), 1 - (15..29 min), 2 - (30..44 min), 3 - (45..59 min)
  *
  * @return the quarter (0..3)
  */
  static int GetQuarter();

  /**
  * Returns minutes from Clock (if is set and ready).
  *
  * @return the minutes (0..59)
  */
  static int GetMin();

  /**
  * Returns seconds from Clock (if is set and ready).
  *
  * @return the seconds (0..59)
  */
  static int GetSec();

  static time_t GetTimeStamp();
  static bool GetLocalTime(struct tm *timeInfo);

  /**
  * Returns pointer of Clock
  *
  * @return pointer of Clock
  */
  static Clock* GetInstance();

  Clock();
  virtual ~Clock();

  virtual bool isReady();
  virtual int getYear();
  virtual int getMonth();
  virtual int getDay();
  virtual int getDayOfWeek();
  virtual enum DayOfWeek getHvacDayOfWeek();
  virtual int getHour();
  virtual int getQuarter();
  virtual int getMin();
  virtual int getSec();
  virtual time_t getTimeStamp();
  virtual bool getLocalTime(struct tm *timeInfo);

  void onTimer() override;
  bool iterateConnected() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void onDeviceConfigChange(uint64_t fieldBit) override;

  virtual void parseLocaltimeFromServer(TSDC_UserLocalTimeResult *result);

  void setUseAutomaticTimeSyncRemoteConfig(bool value);
  void printCurrentTime(const char *prefix = nullptr);

  void setAutomaticTimeSync(bool value) { automaticTimeSync = value; }

 protected:
  void setSystemTime(time_t newTime);
  time_t localtime = {};
  uint32_t lastServerUpdate = 0;
  uint32_t lastMillis = 0;
  bool isClockReady = false;
  bool automaticTimeSync = true;
  bool useAutomaticTimeSyncRemoteConfig = true;
};

};  // namespace Supla

#endif  // SRC_SUPLA_CLOCK_CLOCK_H_
