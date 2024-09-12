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
  static bool IsReady();
  static int GetYear();
  static int GetMonth();
  static int GetDay();
  static int GetDayOfWeek();  // 1 - Sunday, 2 - Monday
  static enum DayOfWeek GetHvacDayOfWeek();  // 0 - Sunday, 1 - Monday as enum
  static int GetHour();
  static int GetQuarter();  // 0 - 0..14 min, 1 - 15..29 min, 2 - 30..44 min, 3
                            // - 45..59 min
  static int GetMin();
  static int GetSec();
  static time_t GetTimeStamp();

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

  void onTimer() override;
  bool iterateConnected() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void onDeviceConfigChange(uint64_t fieldBit) override;

  virtual void parseLocaltimeFromServer(TSDC_UserLocalTimeResult *result);

  void setUseAutomaticTimeSyncRemoteConfig(bool value);
  void printCurrentTime(const char *prefix = nullptr);

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
