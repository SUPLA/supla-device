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

#include <supla/log_wrapper.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <sys/time.h>
#endif

#include "../time.h"
#include "clock.h"

namespace Supla {

Clock::Clock()
    : localtime(0), lastServerUpdate(0), lastMillis(0), isClockReady(false) {
}

bool Clock::isReady() {
  return isClockReady;
}

int Clock::getYear() {
  struct tm timeinfo;
  time_t currentTime = time(0);
  gmtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_year + 1900;
}

int Clock::getMonth() {
  struct tm timeinfo;
  //  timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  gmtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_mon + 1;
}

int Clock::getDay() {
  struct tm timeinfo;
  //  timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  gmtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_mday;
}

int Clock::getDayOfWeek() {
  struct tm timeinfo;
  //  timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  gmtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_wday + 1;  // 1 - Sunday, 2 - Monday ...
}

int Clock::getHour() {
  struct tm timeinfo;
  // timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  gmtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_hour;
}

int Clock::getMin() {
  struct tm timeinfo;
  // timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  gmtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_min;
}

int Clock::getSec() {
  struct tm timeinfo;
  time_t currentTime = time(0);
  gmtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_sec;
}

void Clock::parseLocaltimeFromServer(TSDC_UserLocalTimeResult *result) {
  struct tm timeinfo {};

  isClockReady = true;

  SUPLA_LOG_DEBUG(
            "Current local time: %d-%d-%d %d:%d:%d",
            getYear(),
            getMonth(),
            getDay(),
            getHour(),
            getMin(),
            getSec());

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
  SUPLA_LOG_DEBUG(
            "Received local time from server: %d-%d-%d %d:%d:%d",
            getYear(),
            getMonth(),
            getDay(),
            getHour(),
            getMin(),
            getSec());
}

void Clock::onTimer() {
  if (isClockReady) {
    uint32_t curMillis = millis();
    int seconds = (curMillis - lastMillis) / 1000;
    if (seconds > 0) {
      lastMillis = curMillis - ((curMillis - lastMillis) % 1000);
      for (int i = 0; i < seconds; i++) {
#if defined(ARDUINO_ARCH_AVR)
        system_tick();
#endif
        localtime++;
      }
    }
  }
}

bool Clock::iterateConnected() {
  if (lastServerUpdate == 0 ||
      millis() - lastServerUpdate > 5 * 60000) {  // update every 5 min
    for (auto proto = Supla::Protocol::ProtocolLayer::first();
        proto != nullptr; proto = proto->next()) {
      proto->getUserLocaltime();
    }
    lastServerUpdate = millis();
    return false;
  }
  return true;
}

};  // namespace Supla
