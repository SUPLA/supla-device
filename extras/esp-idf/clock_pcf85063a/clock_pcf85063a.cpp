// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "clock_pcf85063a.h"

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

namespace {
constexpr uint8_t kControl112HourMask = 1U << 1;
constexpr uint8_t kHoursValueMask24 = 0x3F;

bool isBcd(uint8_t value) {
  return (value & 0x0F) <= 9 && ((value >> 4) & 0x0F) <= 9;
}
}  // namespace

namespace Supla {

ClockPCF85063A::ClockPCF85063A(Supla::I2CDriver *driver) : driver(driver) {
}

void ClockPCF85063A::onInit() {
  if (driver == nullptr) {
    return;
  }

  driver->initialize();
  if (!driver->isInitialized()) {
    return;
  }

  driver->aquire();
  devHandle = driver->addDevice(RTC_ADDRESS, I2C_FREQUENCY);
  driver->release();
  if (devHandle == nullptr) {
    SUPLA_LOG_WARNING("ClockPCF85063A[0x%02X]: failed to add I2C device",
                      RTC_ADDRESS);
    return;
  }

  SUPLA_LOG_DEBUG("ClockPCF85063A[0x%02X]: I2C device added", RTC_ADDRESS);
  setRtcTimeToSystem();
  initialized = true;
  SUPLA_LOG_DEBUG("ClockPCF85063A initialized");
}

void ClockPCF85063A::onTimer() {
  // The system time is synchronized from the RTC in iterateAlways().
}

void ClockPCF85063A::iterateAlways() {
  if (!initialized || millis() - lastSyncTimestamp < 60000) {
    return;
  }

  setRtcTimeToSystem();
  lastSyncTimestamp = millis();
}

void ClockPCF85063A::parseLocaltimeFromServer(
    TSDC_UserLocalTimeResult *result) {
  Supla::Clock::parseLocaltimeFromServer(result);
  setSystemTimeToRtc();
}

void ClockPCF85063A::setSystemTimeToRtc() {
  if (driver == nullptr || !driver->isInitialized() || devHandle == nullptr) {
    return;
  }

  time_t currentTime = getTimeStamp();
  if (currentTime < MINIMUM_VALID_TIME) {
    SUPLA_LOG_WARNING("ClockPCF85063A: system time is not valid yet");
    return;
  }

  struct tm timeinfo {};
  gmtime_r(&currentTime, &timeinfo);

  uint8_t control[2] = {};
  control[CONTROL_1_REG] = 0;  // normal mode, 24-hour mode, clock running
  control[CONTROL_2_REG] = 0;  // disable alarm and timer interrupts
  uint8_t timeData[TIME_REGISTER_COUNT] = {};
  timeData[0] = toBcd(timeinfo.tm_sec);
  timeData[1] = toBcd(timeinfo.tm_min);
  timeData[2] = toBcd(timeinfo.tm_hour) & kHoursValueMask24;
  timeData[3] = toBcd(timeinfo.tm_mday);
  // The weekday register is a binary value from 0 (Sunday) to 6 (Saturday).
  timeData[4] = static_cast<uint8_t>(timeinfo.tm_wday);
  timeData[5] = toBcd(timeinfo.tm_mon + 1);
  timeData[6] = toBcd((timeinfo.tm_year + 1900) % 100);

  bool controlWritten = writeRegisters(CONTROL_1_REG, control, sizeof(control));
  bool timeWritten =
      writeRegisters(SECONDS_REG, timeData, sizeof(timeData));
  if (controlWritten && timeWritten) {
    memcpy(data, control, sizeof(control));
    memcpy(data + SECONDS_REG, timeData, sizeof(timeData));
    isClockReady = true;
    SUPLA_LOG_DEBUG("ClockPCF85063A: system time written to RTC");
  }
}

void ClockPCF85063A::setRtcTimeToSystem() {
  uint8_t rtcData[REGISTER_COUNT] = {};
  lastSyncTimestamp = millis();
  if (!readRegisters(rtcData, sizeof(rtcData))) {
    return;
  }

  if (!isValidTime(rtcData)) {
    SUPLA_LOG_WARNING("ClockPCF85063A: invalid time registers");
    return;
  }

  if (isOscillatorStopped(rtcData)) {
    SUPLA_LOG_WARNING("ClockPCF85063A: oscillator stop flag is set");
    if (isClockReady) {
      setSystemTimeToRtc();
    }
    return;
  }

  memcpy(data, rtcData, sizeof(data));
  time_t rtcTime = timeFromRegisters(rtcData);
  if (rtcTime < MINIMUM_VALID_TIME) {
    SUPLA_LOG_WARNING("ClockPCF85063A: RTC time is before year 2000");
    return;
  }

  setSystemTime(rtcTime);
  isClockReady = true;
}

bool ClockPCF85063A::readRegisters(uint8_t *target, size_t size) {
  if (driver == nullptr || !driver->isInitialized() ||
      devHandle == nullptr || target == nullptr || size != REGISTER_COUNT) {
    return false;
  }

  const uint8_t startingRegisterAddress = 0;
  driver->aquire();
  esp_err_t result = i2c_master_transmit_receive(devHandle,
                                                 &startingRegisterAddress,
                                                 sizeof startingRegisterAddress,
                                                 target,
                                                 size,
                                                 400);
  driver->release();
  if (result != ESP_OK) {
    SUPLA_LOG_WARNING("ClockPCF85063A read failed: %s",
                      esp_err_to_name(result));
    return false;
  }
  return true;
}

bool ClockPCF85063A::writeRegisters(uint8_t address,
                                    const uint8_t *source,
                                    size_t size) {
  if (driver == nullptr || !driver->isInitialized() ||
      devHandle == nullptr || source == nullptr || size == 0 ||
      address + size > REGISTER_COUNT) {
    return false;
  }

  uint8_t payload[REGISTER_COUNT + 1] = {};
  payload[0] = address;
  memcpy(payload + 1, source, size);

  driver->aquire();
  esp_err_t result = i2c_master_transmit(devHandle, payload, size + 1, 400);
  driver->release();
  if (result != ESP_OK) {
    SUPLA_LOG_WARNING("ClockPCF85063A write 0x%02X failed: %s",
                      address,
                      esp_err_to_name(result));
    return false;
  }
  return true;
}

bool ClockPCF85063A::isValidTime(const uint8_t *rtcData) const {
  if (rtcData == nullptr ||
      (rtcData[CONTROL_1_REG] & kControl112HourMask) != 0) {
    return false;
  }

  const uint8_t seconds = rtcData[SECONDS_REG] & 0x7F;
  const uint8_t minutes = rtcData[SECONDS_REG + 1];
  const uint8_t hours = rtcData[SECONDS_REG + 2];
  const uint8_t days = rtcData[SECONDS_REG + 3];
  const uint8_t weekdays = rtcData[SECONDS_REG + 4];
  const uint8_t months = rtcData[SECONDS_REG + 5];
  const uint8_t years = rtcData[SECONDS_REG + 6];

  return isBcd(seconds) && fromBcd(seconds) <= 59 && isBcd(minutes) &&
         fromBcd(minutes) <= 59 && isBcd(hours) && fromBcd(hours) <= 23 &&
         isBcd(days) && fromBcd(days) >= 1 && fromBcd(days) <= 31 &&
         weekdays <= 6 && isBcd(months) && fromBcd(months) >= 1 &&
         fromBcd(months) <= 12 && isBcd(years);
}

bool ClockPCF85063A::isOscillatorStopped(const uint8_t *rtcData) const {
  return rtcData != nullptr &&
         (rtcData[SECONDS_REG] & OSCILLATOR_STOP_MASK) != 0;
}

time_t ClockPCF85063A::timeFromRegisters(const uint8_t *rtcData) const {
  struct tm timeinfo {};
  timeinfo.tm_year = 2000 + fromBcd(rtcData[SECONDS_REG + 6]) - 1900;
  timeinfo.tm_mon = fromBcd(rtcData[SECONDS_REG + 5]) - 1;
  timeinfo.tm_mday = fromBcd(rtcData[SECONDS_REG + 3]);
  timeinfo.tm_hour = fromBcd(rtcData[SECONDS_REG + 2]);
  timeinfo.tm_min = fromBcd(rtcData[SECONDS_REG + 1]);
  timeinfo.tm_sec = fromBcd(rtcData[SECONDS_REG] & 0x7F);
  return mktime(&timeinfo);
}

uint8_t ClockPCF85063A::toBcd(int value) {
  return static_cast<uint8_t>((value / 10) * 16 + value % 10);
}

int ClockPCF85063A::fromBcd(uint8_t value) {
  return ((value >> 4) & 0x0F) * 10 + (value & 0x0F);
}

}  // namespace Supla
