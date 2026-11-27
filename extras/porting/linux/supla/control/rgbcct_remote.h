// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_REMOTE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_REMOTE_H_

#include <array>
#include <mutex>  // NOLINT(build/c++11)
#include <optional>
#include <string>

#include "rgbcct_parsed.h"

namespace Supla::Control {

class RgbCctRemote : public LinuxLighting {
 public:
  enum Field { State, WhiteBrightness, WhiteTemperature, RgbBrightness,
               Red, Green, Blue, FieldCount };
  static constexpr std::array<const char *, FieldCount> fieldNames = {
      "state", "white_brightness", "white_temperature", "rgb_brightness",
      "red", "green", "blue",
  };
  using Fields = std::array<std::string, FieldCount>;

  RgbCctRemote(Parser::Parser *parser, Payload::Payload &output,
               Fields fields, uint32_t intervalMs, uint32_t syncTimeoutMs);

  void onInit() override;
  void onSaveState() override;
  void iterateAlways() override;
  void onFastTimer() override {}
  void setRGBCCTValueOnDevice(uint32_t[5], int) override {}
  bool setRuntimeFunction(uint32_t function) override;
  void handleAction(int event, int action) override;
  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *value) override;
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;
  void setRGBCCT(int red, int green, int blue, int rgbBrightness,
                 int whiteBrightness, int whiteTemperature,
                 bool toggle = false, bool instant = false) override;

 private:
  using Values = std::array<int, FieldCount>;
  struct Settings {
    uint32_t function = 0;
    Values values{};
    bool operator==(const Settings &other) const {
      return function == other.function && values == other.values;
    }
  };

  static bool fieldApplies(Field field, uint32_t function);
  Settings readSettings() const;
  std::optional<Values> readFeedback(uint32_t function) const;
  bool feedbackMatches(const Settings &desired, const Values &feedback) const;
  void importFeedback(const Values &feedback);
  bool publish(const Settings &settings);

  Payload::Payload &output;
  const Fields fields;
  const uint32_t intervalMs;
  const uint32_t syncTimeoutMs;
  const bool hasFeedback;

  // Native button actions run on the Linux timer thread.
  std::recursive_mutex lightingMutex;
  Settings observed;
  std::optional<uint32_t> pendingSince;
  std::optional<Settings> lastPublished;
  std::optional<uint32_t> lastAttempt;
  enum class Send : uint8_t { Waiting, Attempted, Sent };
  Send send = Send::Waiting;
  bool localCommand = false;
  bool wasConnected = false;
  std::optional<Values> feedback;
  uint64_t feedbackRevision = 0;
  uint64_t publishedRevision = 0;
};

}  // namespace Supla::Control

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_REMOTE_H_
