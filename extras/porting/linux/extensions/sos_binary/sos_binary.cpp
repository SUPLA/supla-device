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

#include <supla-common/proto.h>
#include <supla/channels/channel.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/time.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include "linux_channel_factory.h"
#include "linux_yaml_config.h"

namespace {

constexpr uint32_t kDefaultShortMs = 200;
constexpr uint32_t kDefaultLongMs = 600;
constexpr uint32_t kDefaultPauseMs = 200;
constexpr uint32_t kMinTimingMs = 10;
constexpr uint32_t kMaxTimingMs = 60000;

struct MorseStep {
  bool state;
  uint32_t durationMs;
};

uint32_t clampTimingMs(int value, uint32_t fallback) {
  if (value <= 0) {
    return fallback;
  }

  return static_cast<uint32_t>(
      std::clamp(value,
                 static_cast<int>(kMinTimingMs),
                 static_cast<int>(kMaxTimingMs)));
}

class SosBinary : public Supla::Sensor::VirtualBinary {
 public:
  SosBinary(uint32_t shortMs, uint32_t longMs, uint32_t pauseMs)
      : steps{{{true, shortMs},
               {false, pauseMs},
               {true, shortMs},
               {false, pauseMs},
               {true, shortMs},
               {false, longMs},
               {true, longMs},
               {false, pauseMs},
               {true, longMs},
               {false, pauseMs},
               {true, longMs},
               {false, longMs},
               {true, shortMs},
               {false, pauseMs},
               {true, shortMs},
               {false, pauseMs},
               {true, shortMs},
               {false, longMs * 3}}} {
    setUseConfiguredTimeout(false);
    setReadIntervalMs(std::max<uint32_t>(kMinTimingMs, pauseMs / 2));
    getChannel()->setDefault(SUPLA_CHANNELFNC_BINARY_SENSOR);
  }

  void onInit() override {
    Supla::Sensor::VirtualBinary::onInit();
    stepIndex = 0;
    stepStartedAt = millis();
    applyCurrentStep();
  }

  void iterateAlways() override {
    uint32_t now = millis();
    if (now - stepStartedAt >= steps[stepIndex].durationMs) {
      stepIndex = (stepIndex + 1) % steps.size();
      stepStartedAt = now;
      applyCurrentStep();
    }

    Supla::Sensor::VirtualBinary::iterateAlways();
  }

 private:
  void applyCurrentStep() {
    if (steps[stepIndex].state) {
      set();
    } else {
      clear();
    }
  }

  std::array<MorseStep, 18> steps;
  std::size_t stepIndex = 0;
  uint32_t stepStartedAt = 0;
};

bool AddSosBinary(const Supla::Linux::ChannelFactoryContext& context) {
  const auto& ch = context.channel;
  auto& config = context.config;

  uint32_t shortMs = kDefaultShortMs;
  uint32_t longMs = kDefaultLongMs;
  uint32_t pauseMs = kDefaultPauseMs;

  if (ch["short_ms"]) {
    config.markChannelParameterUsed();
    shortMs = clampTimingMs(ch["short_ms"].as<int>(), kDefaultShortMs);
  }
  if (ch["long_ms"]) {
    config.markChannelParameterUsed();
    longMs = clampTimingMs(ch["long_ms"].as<int>(), kDefaultLongMs);
  }
  if (ch["pause_ms"]) {
    config.markChannelParameterUsed();
    pauseMs = clampTimingMs(ch["pause_ms"].as<int>(), kDefaultPauseMs);
  }

  auto sensor = new SosBinary(shortMs, longMs, pauseMs);
  return config.addCommonChannelParameters(ch, sensor);
}

}  // namespace

namespace Supla {
namespace Linux {

void initSosBinaryExtension() {
  ChannelFactoryRegistry::instance().registerFactory("sos_binary",
                                                     "sos_binary",
                                                     AddSosBinary);
}

}  // namespace Linux
}  // namespace Supla
