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

#ifndef SRC_SUPLA_CLOCK_DS1307RTC_H_
#define SRC_SUPLA_CLOCK_DS1307RTC_H_

/*
Dependency: https://github.com/adafruit/RTClib,
use library manager to install it
*/

#include <supla/log_wrapper.h>
#include <time.h>
#include "clock.h"

#include "RTClib.h"

namespace Supla {

class DS1307RTC : public Clock {
 public:
  DS1307RTC() {}

  void onInit() {
    if (!rtc.begin()) {
      SUPLA_LOG_DEBUG("Unable to find RTC");
    } else {
      struct tm timeinfo {};
      isRTCReady = true;
      RTCLostPower = (rtc.isrunning()) ? false : true;
      DateTime now = rtc.now();

      timeinfo.tm_year = now.year() - 1900;
      timeinfo.tm_mon = now.month() - 1;
      timeinfo.tm_mday = now.day();
      timeinfo.tm_hour = now.hour();
      timeinfo.tm_min = now.minute();
      timeinfo.tm_sec = now.second();

      localtime = mktime(&timeinfo);

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
      timeval tv = {localtime, 0};
      settimeofday(&tv, nullptr);
#elif defined(ARDUINO_ARCH_AVR)
      set_system_time(mktime(&timeinfo));
#endif
      printCurrentTime("from RTC:");

      if (getYear() >= 2023) {
        isClockReady = true;
      } else {
        SUPLA_LOG_DEBUG("Clock is not ready");
      }
    }
  }

  bool rtcIsReady() {
    return isRTCReady;
  }

  bool getRTCLostPowerFlag() {
    return RTCLostPower;
  }

  void resetRTCLostPowerFlag() {
    RTCLostPower = false;
  }

  void parseLocaltimeFromServer(TSDC_UserLocalTimeResult *result) {
    struct tm timeinfo {};

    isClockReady = true;

    printCurrentTime("current");

    timeinfo.tm_year = result->year - 1900;
    timeinfo.tm_mon = result->month - 1;
    timeinfo.tm_mday = result->day;
    timeinfo.tm_hour = result->hour;
    timeinfo.tm_min = result->min;
    timeinfo.tm_sec = result->sec;

    localtime = mktime(&timeinfo);

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
    timeval tv = {localtime, 0};
    settimeofday(&tv, nullptr);
#elif defined(ARDUINO_ARCH_AVR)
    set_system_time(mktime(&timeinfo));
#endif
    printCurrentTime("new");

    //  Update RTC if minutes or seconds are different
    //  from the time obtained from the server
    if (isRTCReady) {
      DateTime now = rtc.now();
      if ((now.year() != getYear()) || (now.month() != getMonth()) ||
          (now.day() != getDay()) || (now.hour() != getHour()) ||
          (now.minute() != getMin()) || (now.second() - getSec() > 5) ||
          (now.second() - getSec() < -5)) {
        rtc.adjust(DateTime(getTimeStamp()));
        SUPLA_LOG_DEBUG("Update RTC time from server");
      }
    }
}

 protected:
  RTC_DS1307 rtc;
  bool RTCLostPower = false;
  bool isRTCReady = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_CLOCK_DS1307RTC_H_
