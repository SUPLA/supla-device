// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_CLOCK_PCF85063A_CLOCK_PCF85063A_H_
#define EXTRAS_ESP_IDF_CLOCK_PCF85063A_CLOCK_PCF85063A_H_

#include <stddef.h>
#include <stdint.h>

#include <driver/i2c_types.h>
#include <esp_i2c_driver.h>
#include <supla/clock/clock.h>

namespace Supla {

class ClockPCF85063A : public Clock {
 public:
  explicit ClockPCF85063A(Supla::I2CDriver *driver);
  ~ClockPCF85063A() override = default;

  void onInit() override;
  void onTimer() override;
  void iterateAlways() override;
  void parseLocaltimeFromServer(TSDC_UserLocalTimeResult *result) override;

  void setSystemTimeToRtc();
  void setRtcTimeToSystem();

 private:
  bool readRegisters(uint8_t *data, size_t size);
  bool writeRegisters(uint8_t address, const uint8_t *data, size_t size);
  bool isValidTime(const uint8_t *data) const;
  bool isOscillatorStopped(const uint8_t *data) const;
  time_t timeFromRegisters(const uint8_t *data) const;

  static uint8_t toBcd(int value);
  static int fromBcd(uint8_t value);

  static constexpr uint8_t RTC_ADDRESS = 0x51;
  static constexpr uint32_t I2C_FREQUENCY = 400000;
  static constexpr uint8_t CONTROL_1_REG = 0x00;
  static constexpr uint8_t CONTROL_2_REG = 0x01;
  static constexpr uint8_t SECONDS_REG = 0x04;
  static constexpr uint8_t REGISTER_COUNT = 11;
  static constexpr uint8_t TIME_REGISTER_COUNT = 7;
  static constexpr uint8_t OSCILLATOR_STOP_MASK = 1U << 7;
  static constexpr time_t MINIMUM_VALID_TIME = 946684800;  // 2000-01-01

  Supla::I2CDriver *driver = nullptr;
  i2c_master_dev_handle_t devHandle = nullptr;
  uint8_t data[REGISTER_COUNT] = {};
  uint32_t lastSyncTimestamp = 0;
  bool initialized = false;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_CLOCK_PCF85063A_CLOCK_PCF85063A_H_
