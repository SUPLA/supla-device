// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rgbcct_parsed.h"

#include <supla/payload/payload.h>
#include <supla/time.h>

#include <algorithm>
#include <mutex>  // NOLINT(build/c++11)

#include <nlohmann/json.hpp>

namespace Supla::Control {
namespace {

// In native physical-output order by capacity, as in LightingPwmLeds.
constexpr struct {
  uint32_t function;
  uint32_t bit;
  int outputs;
} layouts[] = {
    {SUPLA_CHANNELFNC_DIMMER, SUPLA_RGBW_BIT_FUNC_DIMMER, 1},
    {SUPLA_CHANNELFNC_DIMMER_CCT, SUPLA_RGBW_BIT_FUNC_DIMMER_CCT, 2},
    {SUPLA_CHANNELFNC_RGBLIGHTING, SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING, 3},
    {SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING,
     SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING, 4},
    {SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
     SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB, 5},
};

}  // namespace

int LinuxLighting::outputCount(uint32_t function) {
  for (const auto &layout : layouts) {
    if (layout.function == function) {
      return layout.outputs;
    }
  }
  return 0;
}

LinuxLighting::LinuxLighting(Parser::Parser *parser)
    : Sensor::SensorParsed<LightingPwmBase>(parser) {
}

RgbCctParsed::RgbCctParsed(Parser::Parser *parser) : LinuxLighting(parser) {
}

void LinuxLighting::setLayout(uint32_t function) {
  const int capacity = outputCount(function);
  uint32_t functions = 0;
  for (const auto &layout : layouts) {
    if (layout.outputs <= capacity) {
      functions |= layout.bit;
    }
  }
  getChannel()->setFuncList(functions);
  setFunction(function);
  layoutConfigured = true;
}

bool LinuxLighting::supportsFunction(uint32_t function) const {
  if (function == SUPLA_CHANNELFNC_NONE) {
    return true;
  }
  for (const auto &layout : layouts) {
    if (layout.function == function) {
      return (getChannel()->getFuncList() & layout.bit) != 0;
    }
  }
  return false;
}

bool LinuxLighting::setRuntimeFunction(uint32_t function) {
  return (!layoutConfigured || supportsFunction(function)) &&
         LightingPwmBase::setRuntimeFunction(function);
}

void LinuxLighting::onLoadConfig(SuplaDeviceClass *sdc) {
  const auto defaultFunction = getChannel()->getDefaultFunction();
  LightingPwmBase::onLoadConfig(sdc);
  if (layoutConfigured &&
      !supportsFunction(getChannel()->getDefaultFunction())) {
    setFunction(defaultFunction);
  }
}

void LinuxLighting::setButtonControl(ButtonControlType control) {
  configuredButtonControl = control;
}

void LinuxLighting::onInit() {
  if (configuredButtonControl) {
    buttonControlType = *configuredButtonControl;
  }
  LightingPwmBase::onInit();
}

void RgbCctParsed::setPwmOutput(Payload::Payload *payload,
                               uint32_t intervalMs) {
  pwmPayload = payload;
  outputIntervalMs = intervalMs;
}

void RgbCctParsed::iterateAlways() {
  {
    std::unique_lock<std::mutex> layoutLock(layoutMutex, std::defer_lock);
    if (pwmPayload &&
        getChannel()->getDefaultFunction() != previousChannelFunction) {
      // Wait for any in-flight callback before discarding its snapshot.
      // Keep the timer out until the native function-change reset is done.
      layoutLock.lock();
      std::lock_guard<std::mutex> lock(pwmMutex);
      latestPwm = {};
    }
    LightingPwmBase::iterateAlways();
  }
  publishPwm();

  if (parser && (millis() - lastReadTime > 100)) {
    if (setOfflineIfSourceDisconnected()) {
      lastReadTime = millis();
      return;
    }
    refreshParserSource(false);
    lastReadTime = millis();
    setChannelStateOnline(!isOffline());
  }
}

void RgbCctParsed::onFastTimer() {
  if (!pwmPayload) {
    LightingPwmBase::onFastTimer();
    return;
  }

  std::unique_lock<std::mutex> lock(layoutMutex, std::try_to_lock);
  if (lock && getChannel()->getDefaultFunction() == previousChannelFunction) {
    LightingPwmBase::onFastTimer();
  }
}

void RgbCctParsed::setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) {
  if (!pwmPayload) {
    return;
  }

  PwmState state;
  state.activeOutputs = std::clamp(usedOutputs, 0, 5);
  std::copy_n(output, state.activeOutputs, state.values.begin());
  // The timer and the main loop may both call this. Keep the lock scoped
  // to the fixed-size snapshot, without serialization or I/O.
  std::lock_guard<std::mutex> lock(pwmMutex);
  latestPwm = state;
}

void RgbCctParsed::publishPwm() {
  if (!pwmPayload || !initDone) {
    return;
  }

  const uint32_t now = millis();
  if (lastPublishAttempt && now - *lastPublishAttempt < outputIntervalMs) {
    return;
  }

  PwmState state;
  const int activeOutputs = isChannelSuspended()
      ? 0 : outputCount(getChannel()->getDefaultFunction());
  if (activeOutputs != 0) {
    std::lock_guard<std::mutex> lock(pwmMutex);
    if (latestPwm.activeOutputs != activeOutputs) {
      // Wait for the native timer to calculate the new layout.
      return;
    }
    state = latestPwm;
  }
  if (publishedPwm && *publishedPwm == state) {
    return;
  }

  lastPublishAttempt = now;
  if (pwmPayload->publishState({
          {"pwm", state.values},
          {"active_outputs", state.activeOutputs},
          {"max_pwm", maxHwValue},
      })) {
    publishedPwm = state;
  } else {
    publishedPwm.reset();
  }
}

}  // namespace Supla::Control
