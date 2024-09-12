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
#include <supla/storage/config.h>
#include <supla/device/remote_device_config.h>
#include <SuplaDevice.h>

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32) || \
    defined(ESP8266) || defined(ESP32) || defined(ESP_PLATFORM)
#include <sys/time.h>
#endif

#include "../time.h"
#include "clock.h"

namespace Supla {

static Clock *clockInstance = nullptr;

bool Clock::IsReady() {
  if (clockInstance && clockInstance->isReady()) {
    return true;
  }
  return false;
}

int Clock::GetYear() {
  if (IsReady()) {
    return clockInstance->getYear();
  }
  return 0;
}

int Clock::GetMonth() {
  if (IsReady()) {
    return clockInstance->getMonth();
  }
  return 0;
}

int Clock::GetDay() {
  if (IsReady()) {
    return clockInstance->getDay();
  }
  return 0;
}

int Clock::GetDayOfWeek() {
  if (IsReady()) {
    return clockInstance->getDayOfWeek();
  }
  return 0;
}

enum DayOfWeek Clock::GetHvacDayOfWeek() {
  if (IsReady()) {
    return clockInstance->getHvacDayOfWeek();
  }
  return DayOfWeek_Sunday;
}

int Clock::GetHour() {
  if (IsReady()) {
    return clockInstance->getHour();
  }
  return 0;
}

int Clock::GetQuarter() {
  if (IsReady()) {
    return clockInstance->getQuarter();
  }
  return 0;
}

int Clock::GetMin() {
  if (IsReady()) {
    return clockInstance->getMin();
  }
  return 0;
}

int Clock::GetSec() {
  if (IsReady()) {
    return clockInstance->getSec();
  }
  return 0;
}

time_t Clock::GetTimeStamp() {
  if (IsReady()) {
    return clockInstance->getTimeStamp();
  }
  return 0;
}

Clock* Clock::GetInstance() {
  return clockInstance;
}

Clock::Clock() {
  clockInstance = this;
}

Clock::~Clock() {
  clockInstance = nullptr;
}

bool Clock::isReady() {
  return isClockReady;
}

int Clock::getYear() {
  struct tm timeinfo;
  time_t currentTime = time(0);
  localtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_year + 1900;
}

int Clock::getMonth() {
  struct tm timeinfo;
  //  timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  localtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_mon + 1;
}

int Clock::getDay() {
  struct tm timeinfo;
  //  timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  localtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_mday;
}

int Clock::getDayOfWeek() {
  struct tm timeinfo;
  //  timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  localtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_wday + 1;  // 1 - Sunday, 2 - Monday ...
}

enum DayOfWeek Clock::getHvacDayOfWeek() {
  return static_cast<enum DayOfWeek>(getDayOfWeek() - 1);
}

int Clock::getHour() {
  struct tm timeinfo;
  // timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  localtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_hour;
}

int Clock::getQuarter() {
  return getMin() / 15;
}

int Clock::getMin() {
  struct tm timeinfo;
  // timeinfo = gmtime(time(0));
  time_t currentTime = time(0);
  localtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_min;
}

int Clock::getSec() {
  struct tm timeinfo;
  time_t currentTime = time(0);
  localtime_r(&currentTime, &timeinfo);
  return timeinfo.tm_sec;
}

time_t Clock::getTimeStamp() {
  time_t currentTime = time(0);
  return currentTime;
}

void Clock::parseLocaltimeFromServer(TSDC_UserLocalTimeResult *result) {
  struct tm timeinfo {};
  printCurrentTime("current");
  timeinfo.tm_year = result->year - 1900;
  timeinfo.tm_mon = result->month - 1;
  timeinfo.tm_mday = result->day;
  timeinfo.tm_hour = result->hour;
  timeinfo.tm_min = result->min;
  timeinfo.tm_sec = result->sec;

  time_t newTime = mktime(&timeinfo);

  setSystemTime(newTime);

  isClockReady = true;
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
  if (!automaticTimeSync) {
    return true;
  }

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

void Clock::onLoadConfig(SuplaDeviceClass *sdc) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    if (useAutomaticTimeSyncRemoteConfig) {
      // register DeviceConfig field bit:
      Supla::Device::RemoteDeviceConfig::RegisterConfigField(
          SUPLA_DEVICE_CONFIG_FIELD_AUTOMATIC_TIME_SYNC);
      if (sdc) {
        sdc->addFlags(SUPLA_DEVICE_FLAG_CALCFG_SET_TIME);
      }

      // load automaticTimeSync from storage
      uint8_t value = 1;
      cfg->getUInt8(Supla::AutomaticTimeSyncCfgTag, &value);
      automaticTimeSync = (value == 1);
      SUPLA_LOG_DEBUG("Clock: automaticTimeSync: %d", automaticTimeSync);
    }
  }
}

void Clock::onDeviceConfigChange(uint64_t fieldBit) {
  if (fieldBit == SUPLA_DEVICE_CONFIG_FIELD_AUTOMATIC_TIME_SYNC) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      uint8_t value = 1;
      cfg->getUInt8(Supla::AutomaticTimeSyncCfgTag, &value);
      automaticTimeSync = (value == 1);
    }
  }
}

void Clock::setSystemTime(time_t newTime) {
  if (newTime == 0) {
    SUPLA_LOG_WARNING("Clock: failed to set new time");
    return;
  }
  localtime = newTime;
  time_t currentTime = getTimeStamp();
  if (currentTime != localtime) {
#if defined(ARDUINO_ARCH_AVR)
    set_system_time(localtime);
#elif defined(SUPLA_LINUX)
    SUPLA_LOG_DEBUG("Ignore setting time");
#else
    timeval tv = {localtime, 0};
    settimeofday(&tv, nullptr);
#endif
    SUPLA_LOG_DEBUG("Clock: time diff (system-new): %d s",
                    currentTime - newTime);

    printCurrentTime("new");
  }
}

void Clock::setUseAutomaticTimeSyncRemoteConfig(bool value) {
  useAutomaticTimeSyncRemoteConfig = value;
}

void Clock::printCurrentTime(const char *prefix) {
  SUPLA_LOG_DEBUG("Clock:%s%s time: %d-%02d-%02d %02d:%02d:%02d",
                  prefix ? " " : "",
                  prefix ? prefix : "",
                  getYear(),
                  getMonth(),
                  getDay(),
                  getHour(),
                  getMin(),
                  getSec());
}

};  // namespace Supla
