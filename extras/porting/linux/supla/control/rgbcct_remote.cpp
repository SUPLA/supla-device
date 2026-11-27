// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rgbcct_remote.h"

#include <supla/actions.h>
#include <supla/log_wrapper.h>
#include <supla/payload/payload.h>
#include <supla/time.h>

#include <algorithm>
#include <cmath>
#include <mutex>  // NOLINT(build/c++11)
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace Supla::Control {

RgbCctRemote::RgbCctRemote(Parser::Parser *parser, Payload::Payload &output,
                           Fields fields, uint32_t intervalMs,
                           uint32_t syncTimeoutMs)
    : LinuxLighting(parser), output(output), fields(std::move(fields)),
      intervalMs(intervalMs), syncTimeoutMs(syncTimeoutMs),
      hasFeedback(std::any_of(this->fields.begin(), this->fields.end(),
                             [](const auto &key) { return !key.empty(); })) {
}

void RgbCctRemote::onInit() {
  LinuxLighting::onInit();
  observed = readSettings();
  localCommand = !hasFeedback && stateOnInit != RGBW_STATE_ON_INIT_RESTORE;
  if (hasFeedback) {
    setChannelStateOnline(false);
  }
}

void RgbCctRemote::onSaveState() {
  std::lock_guard<std::recursive_mutex> lock(lightingMutex);
  LightingPwmBase::onSaveState();
}

bool RgbCctRemote::setRuntimeFunction(uint32_t function) {
  std::lock_guard<std::recursive_mutex> lock(lightingMutex);
  return LinuxLighting::setRuntimeFunction(function);
}

void RgbCctRemote::handleAction(int event, int action) {
  std::lock_guard<std::recursive_mutex> lock(lightingMutex);
  LightingPwmBase::handleAction(event, action);
}

int32_t RgbCctRemote::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *value) {
  std::lock_guard<std::recursive_mutex> lock(lightingMutex);
  return LightingPwmBase::handleNewValueFromServer(value);
}

void RgbCctRemote::fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) {
  std::lock_guard<std::recursive_mutex> lock(lightingMutex);
  LightingPwmBase::fillSuplaChannelNewValue(value);
}

void RgbCctRemote::setRGBCCT(int red, int green, int blue, int rgbBrightness,
                            int whiteBrightness, int whiteTemperature,
                            bool toggle, bool instant) {
  std::lock_guard<std::recursive_mutex> lock(lightingMutex);
  LightingPwmBase::setRGBCCT(red, green, blue, rgbBrightness, whiteBrightness,
                             whiteTemperature, toggle, instant);
  localCommand = true;
}

bool RgbCctRemote::fieldApplies(Field field, uint32_t function) {
  const int count = outputCount(function);
  switch (field) {
    case State: return count != 0;
    case WhiteBrightness: return count != 0 && count != 3;
    case WhiteTemperature: return count == 2 || count == 5;
    default: return count >= 3;
  }
}

RgbCctRemote::Settings RgbCctRemote::readSettings() const {
  Settings result{getChannel()->getDefaultFunction(),
                  {0, requested.whiteBrightness, requested.whiteTemperature,
                   requested.colorBrightness, requested.red, requested.green,
                   requested.blue}};
  for (int i = 0; i < FieldCount; ++i) {
    if (!fieldApplies(static_cast<Field>(i), result.function)) {
      result.values[i] = 0;
    }
  }
  result.values[State] = result.values[WhiteBrightness] != 0 ||
                         result.values[RgbBrightness] != 0;
  return result;
}

std::optional<RgbCctRemote::Values> RgbCctRemote::readFeedback(
    uint32_t function) const {
  Values values;
  values.fill(-1);
  bool mapped = false;
  for (int i = 0; i < FieldCount; ++i) {
    if (fields[i].empty() || !fieldApplies(static_cast<Field>(i), function)) {
      continue;
    }
    mapped = true;
    const auto raw = parser->getNumberOrText(fields[i]);
    if (!raw) {
      return std::nullopt;
    }
    if (i == State) {
      if (const auto *boolean = std::get_if<bool>(&*raw)) {
        values[i] = *boolean;
        continue;
      }
      if (const auto *str = std::get_if<std::string>(&*raw)) {
        if (*str == "on" || *str == "true" || *str == "1") {
          values[i] = 1;
          continue;
        }
        if (*str == "off" || *str == "false" || *str == "0") {
          values[i] = 0;
          continue;
        }
      }
    }
    const auto *number = std::get_if<double>(&*raw);
    const int maximum = i == State ? 1 : i < Red ? 100 : 255;
    if (!number || !std::isfinite(*number) || *number < 0 ||
        *number > maximum || *number != std::trunc(*number)) {
      return std::nullopt;
    }
    values[i] = *number;
  }
  if (values[State] == 1 && values[WhiteBrightness] <= 0 &&
      values[RgbBrightness] <= 0 &&
      (!fieldApplies(WhiteBrightness, function) ||
       values[WhiteBrightness] == 0) &&
      (!fieldApplies(RgbBrightness, function) ||
       values[RgbBrightness] == 0)) {
    return std::nullopt;
  }
  return mapped ? std::optional<Values>(values) : std::nullopt;
}

bool RgbCctRemote::feedbackMatches(const Settings &desired,
                                  const Values &values) const {
  for (int i = 0; i < FieldCount; ++i) {
    if (values[i] < 0) {
      continue;
    }
    if (values[State] == 0 && (i == WhiteBrightness || i == RgbBrightness)) {
      continue;
    }
    if (desired.values[i] != values[i]) {
      return false;
    }
  }
  return true;
}

void RgbCctRemote::importFeedback(const Values &values) {
  // Clear the brightness hidden by the layout, so native toggle sees "off".
  const auto function = getChannel()->getDefaultFunction();
  auto white = fieldApplies(WhiteBrightness, function)
      ? values[WhiteBrightness] : 0;
  auto rgb = fieldApplies(RgbBrightness, function) ? values[RgbBrightness] : 0;
  if (values[State] == 0) {
    if (white > 0) {
      lastNonZero.whiteBrightness = white;
    }
    if (rgb > 0) {
      lastNonZero.colorBrightness = rgb;
    }
    white = 0;
    rgb = 0;
  } else if (values[State] == 1) {
    const auto current = readSettings();
    if ((white >= 0 ? white : current.values[WhiteBrightness]) == 0 &&
        (rgb >= 0 ? rgb : current.values[RgbBrightness]) == 0) {
      if (white < 0 && fieldApplies(WhiteBrightness, current.function)) {
        white = lastNonZero.whiteBrightness;
      }
      if (rgb < 0 && fieldApplies(RgbBrightness, current.function)) {
        rgb = lastNonZero.colorBrightness;
      }
    }
  }
  LightingPwmBase::setRGBCCT(values[Red], values[Green], values[Blue], rgb,
                             white, values[WhiteTemperature]);
  observed = readSettings();
}

bool RgbCctRemote::publish(const Settings &settings) {
  nlohmann::ordered_json value = nlohmann::ordered_json::object();
  for (int i = 0; i < FieldCount; ++i) {
    if (fieldApplies(static_cast<Field>(i), settings.function)) {
      if (i == State) {
        value[fieldNames[i]] = settings.values[i] != 0;
      } else {
        value[fieldNames[i]] = settings.values[i];
      }
    }
  }
  return output.publishState(value);
}

void RgbCctRemote::iterateAlways() {
  const bool connected = hasFeedback && parser->isSourceConnected();
  if (connected) {
    parser->refreshParserSource();
  }

  std::optional<Settings> toPublish;
  const uint32_t now = millis();
  {
    std::lock_guard<std::recursive_mutex> lock(lightingMutex);
    const auto function = getChannel()->getDefaultFunction();
    const bool functionChanged = function != previousChannelFunction;
    LightingPwmBase::iterateAlways();
    // onFastTimer() is disabled, so held-button dimming runs here.
    if (const int action = getAutoIterateAction(now)) {
      handleAction(0, action);
    }
    const bool active = outputCount(function) && !isChannelSuspended();

    if (functionChanged || (wasConnected && !connected)) {
      // A new function or lost source invalidates previous pending commands.
      pendingSince.reset();
      lastPublished.reset();
      feedback.reset();
      if (!connected && parser) {
        feedbackRevision = parser->getRefreshRevision();
      }
      if (functionChanged) {
        // The native reset is not an external command.
        observed = readSettings();
        localCommand = false;
      }
    }
    wasConnected = connected;

    bool feedbackChanged = false;
    // A cached fallback read is not new feedback; keep the previous sample.
    if (connected && !parser->lastReadUsedFallback() &&
        (functionChanged || feedbackRevision != parser->getRefreshRevision())) {
      feedbackRevision = parser->getRefreshRevision();
      auto sample = readFeedback(function);
      feedbackChanged = sample != feedback;
      feedback = std::move(sample);
    }
    if (hasFeedback && active) {
      setChannelStateOnline(connected && feedback.has_value());
    }

    const auto desired = readSettings();
    const bool changed = !(observed == desired);
    const bool repeatRequest = localCommand && !pendingSince &&
        (!lastPublished || !(*lastPublished == desired) ||
         (feedback && !feedbackMatches(desired, *feedback)));
    if (!active) {
      pendingSince.reset();
    } else if (changed || repeatRequest) {
      pendingSince = now;
      send = Send::Waiting;
    }
    localCommand = false;
    observed = desired;

    // Pending command: Waiting -> Attempted (first publish try starts the
    // sync timeout) -> Sent (output accepted). It ends when matching new
    // feedback arrives, without feedback once sent, or on sync timeout.
    if (pendingSince) {
      if (send == Send::Sent && (!hasFeedback ||
          (feedback && feedbackRevision != publishedRevision &&
           feedbackMatches(desired, *feedback)))) {
        pendingSince.reset();
        feedbackChanged = false;
      } else if (send != Send::Waiting &&
                 now - *pendingSince >= syncTimeoutMs) {
        if (send == Send::Attempted) {
          SUPLA_LOG_WARNING("RgbCctRemote[%d] output retry window expired",
                            getChannelNumber());
        }
        pendingSince.reset();
        feedbackChanged = true;
      } else if (send != Send::Sent &&
                 (!lastAttempt || now - *lastAttempt >= intervalMs)) {
        if (send == Send::Waiting) {
          pendingSince = now;
          send = Send::Attempted;
        }
        lastAttempt = now;
        toPublish = desired;
      }
    }
    if (active && !pendingSince && feedbackChanged && feedback) {
      importFeedback(*feedback);
    }
  }

  if (toPublish && publish(*toPublish)) {
    lastPublished = *toPublish;
    publishedRevision = feedbackRevision;
    send = Send::Sent;
  }
}

}  // namespace Supla::Control
