// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_PARSED_H_

#include <supla/control/lighting_pwm_base.h>
#include <supla/sensor/sensor_parsed.h>

#include <array>
#include <mutex>  // NOLINT(build/c++11)
#include <optional>

namespace Supla {
namespace Payload {
class Payload;
}
namespace Control {
class LinuxLighting : public Sensor::SensorParsed<LightingPwmBase> {
 public:
  explicit LinuxLighting(Parser::Parser *parser);

  void onInit() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void setLayout(uint32_t function);
  bool supportsFunction(uint32_t function) const;
  bool setRuntimeFunction(uint32_t function) override;
  void setButtonControl(ButtonControlType control);

 protected:
  static int outputCount(uint32_t function);

 private:
  std::optional<ButtonControlType> configuredButtonControl;
  bool layoutConfigured = false;
};

class RgbCctParsed : public LinuxLighting {
 public:
  explicit RgbCctParsed(Parser::Parser *parser);

  void iterateAlways() override;
  void onFastTimer() override;
  void setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) override;
  void setPwmOutput(Payload::Payload *payload, uint32_t intervalMs);

 protected:
  uint32_t lastReadTime = 0;

 private:
  struct PwmState {
    std::array<uint32_t, 5> values{};
    int activeOutputs = 0;

    bool operator==(const PwmState &other) const {
      return activeOutputs == other.activeOutputs && values == other.values;
    }
  };

  void publishPwm();

  Payload::Payload *pwmPayload = nullptr;
  uint32_t outputIntervalMs = 250;
  std::mutex layoutMutex;
  std::mutex pwmMutex;
  PwmState latestPwm;
  std::optional<PwmState> publishedPwm;
  std::optional<uint32_t> lastPublishAttempt;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_PARSED_H_
