// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include <supla/network/html/roller_shutter_parameters.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/control/roller_shutter.h>
#include <supla/log_wrapper.h>
#include <supla/network/html/channel_function_parameters.h>
#include <supla/network/web_sender.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

#include "supla/storage/config.h"
#include "supla/storage/config_tags.h"

using Supla::Html::RollerShutterParameters;

namespace {

bool isRollerShutterFunction(uint32_t function) {
  switch (function) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
    case SUPLA_CHANNELFNC_VERTICAL_BLIND:
      return true;
  }
  return false;
}

bool parseTimeMs(const char* value, uint32_t* result) {
  if (value == nullptr || result == nullptr || value[0] == 0) {
    return false;
  }

  uint32_t wholeSeconds = 0;
  uint32_t fraction = 0;
  bool decimalPointFound = false;
  bool digitFound = false;
  bool fractionDigitFound = false;

  for (const char* current = value; *current != 0; current++) {
    if (*current >= '0' && *current <= '9') {
      digitFound = true;
      if (decimalPointFound) {
        if (fractionDigitFound) {
          return false;
        }
        fraction = static_cast<uint32_t>(*current - '0');
        fractionDigitFound = true;
      } else {
        if (wholeSeconds > RS_MAX_OPERATION_TIME_MS / 1000 / 10) {
          return false;
        }
        wholeSeconds = wholeSeconds * 10 + (*current - '0');
      }
    } else if (*current == '.' || *current == ',') {
      if (decimalPointFound || !digitFound) {
        return false;
      }
      decimalPointFound = true;
    } else {
      return false;
    }
  }

  if (!digitFound || (decimalPointFound && !fractionDigitFound) ||
      wholeSeconds > RS_MAX_OPERATION_TIME_MS / 1000) {
    return false;
  }

  const uint32_t milliseconds = wholeSeconds * 1000 + fraction * 100;
  if (milliseconds > RS_MAX_OPERATION_TIME_MS) {
    return false;
  }
  *result = milliseconds;
  return true;
}

bool parseUInt32Strict(const char* value, uint32_t* result) {
  if (value == nullptr || result == nullptr || value[0] == 0) {
    return false;
  }

  uint64_t parsed = 0;
  for (const char* current = value; *current != 0; current++) {
    if (*current < '0' || *current > '9') {
      return false;
    }
    const uint32_t digit = static_cast<uint32_t>(*current - '0');
    if (parsed > (UINT32_MAX - digit) / 10) {
      return false;
    }
    parsed = parsed * 10 + digit;
  }
  *result = static_cast<uint32_t>(parsed);
  return true;
}

void sendDynamicVisibilityScript(Supla::WebSender *sender,
                                 const char *containerId,
                                 const char *functionSelectId) {
  if (sender == nullptr || containerId == nullptr ||
      functionSelectId == nullptr) {
    return;
  }

  char script[560] = {};
  snprintf(script,
           sizeof(script),
           "<script>"
           "(function(){"
           "function u(){"
           "var s=document.getElementById('%s'),"
           "b=document.getElementById('%s');"
           "if(!s||!b)return;"
           "var v=s.value;"
           "b.style.display=("
           "v=='%d'||v=='%d'||v=='%d'||v=='%d'||v=='%d'||v=='%d'||"
           "v=='%d'||v=='%d')?'block':'none';"
           "}"
           "var s=document.getElementById('%s');"
           "if(s){s.addEventListener('change',u);}"
           "u();"
           "})();"
           "</script>",
           functionSelectId,
           containerId,
           SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER,
           SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW,
           SUPLA_CHANNELFNC_TERRACE_AWNING,
           SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR,
           SUPLA_CHANNELFNC_CURTAIN,
           SUPLA_CHANNELFNC_PROJECTOR_SCREEN,
           SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND,
           SUPLA_CHANNELFNC_VERTICAL_BLIND,
           functionSelectId);
  sender->send(script);
}

}  // namespace

RollerShutterParameters::RollerShutterParameters(
    Supla::Control::RollerShutter* rs)
    : HtmlElement(HTML_SECTION_FORM), rs(rs) {
}

RollerShutterParameters::~RollerShutterParameters() {
}

void RollerShutterParameters::setRsPtr(Supla::Control::RollerShutter* rs) {
  this->rs = rs;
}

void RollerShutterParameters::setShowChannelFunction(bool show) {
  showChannelFunction = show;
}

void RollerShutterParameters::setRenderContainer(bool render) {
  renderContainer = render;
}

void RollerShutterParameters::setShowOnlyForRollerFunction(bool showOnly) {
  showOnlyForRollerFunction = showOnly;
}

void RollerShutterParameters::setDynamicVisibilityFromChannelFunction(
    bool enabled) {
  dynamicVisibilityFromChannelFunction = enabled;
}

void RollerShutterParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (rs == nullptr || rs->getChannel() == nullptr || cfg == nullptr) {
    return;
  }
  auto emitField = [&](const char* keyName, const char* label, auto&& render) {
    sender->formField([&]() {
      sender->labelFor(keyName, label);
      sender->tag("div").body([&]() { render(); });
    });
  };

  auto emitSelectField =
      [&](const char* keyName, const char* label, auto&& renderOptions) {
        emitField(keyName, label, [&]() {
          auto select = sender->selectTag(keyName, keyName);
          select.body([&]() { renderOptions(); });
        });
      };

  auto emitYesNoField =
      [&](const char* keyName, const char* label, bool yesSelected) {
        emitSelectField(keyName, label, [&]() {
          sender->selectOption(0, "NO", !yesSelected);
          sender->selectOption(1, "YES", yesSelected);
        });
      };

  char key[16] = {};
  int32_t channelFunc = rs->getChannel()->getDefaultFunction();
  if (showOnlyForRollerFunction && !dynamicVisibilityFromChannelFunction &&
      !isRollerShutterFunction(channelFunc)) {
    return;
  }

  if (renderContainer) {
    char tmp[100] = {};
    snprintf(tmp,
             sizeof(tmp),
             "%s #%d",
             Supla::getRelayChannelName(channelFunc),
             rs->getChannelNumber());

    sender->send("</div><div class=\"box\">");
    sender->tag("h3").body(tmp);
  }

  if (showChannelFunction) {
    rs->generateKey(key, Supla::ConfigTag::ChannelFunctionTag);
    Supla::Html::ChannelFunctionParameters::renderSelectField(
        sender, rs->getChannel(), key, "Channel function");
  }

  char dynamicContainerId[24] = {};
  char dynamicFunctionKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  if (dynamicVisibilityFromChannelFunction) {
    rs->generateKey(dynamicFunctionKey, Supla::ConfigTag::ChannelFunctionTag);
    snprintf(dynamicContainerId,
             sizeof(dynamicContainerId),
             "rs_params_%d",
             rs->getChannelNumber());
    char container[80] = {};
    snprintf(container,
             sizeof(container),
             "<div id=\"%s\">",
             dynamicContainerId);
    sender->send(container);
  }

  if (rs->getMotorUpsideDown() != 0) {
    rs->generateKey(key, Supla::ConfigTag::RollerShutterMotorUpsideDownTag);
    emitYesNoField(key, "Motor upside down", rs->getMotorUpsideDown() == 2);
  }

  if (rs->getButtonsUpsideDown() != 0) {
    rs->generateKey(key, Supla::ConfigTag::RollerShutterButtonsUpsideDownTag);
    emitYesNoField(key, "Buttons upside down", rs->getButtonsUpsideDown() == 2);
  }

  rs->generateKey(key, Supla::ConfigTag::RollerShutterTimeMarginTag);
  emitField(key, "Time margin (%)", [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "number")
        .attr("min", -1)
        .attr("max", 100)
        .attr("step", 1)
        .attr("placeholder", "Use -1 for default")
        .attr("name", key)
        .attr("id", key)
        .attr("value", static_cast<int>(rs->getTimeMargin()))
        .finish();
  });

  rs->generateKey(key, Supla::ConfigTag::RollerShutterOpeningTimeTag);
  emitField(key, "Full opening time (sec.)", [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "number")
        .attr("min", 0)
        .attr("max", 300)
        .attr("step", 1, 1)
        .attr("name", key)
        .attr("id", key);
    if (rs->isAutoCalibrationSupported()) {
      input.attr("placeholder", "Use 0 for autocalibration");
    }
    input.attr("value", rs->getOpeningTimeMs() / 100, 1).finish();
  });

  rs->generateKey(key, Supla::ConfigTag::RollerShutterClosingTimeTag);
  emitField(key, "Full closing time (sec.)", [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "number")
        .attr("min", 0)
        .attr("max", 300)
        .attr("step", 1, 1)
        .attr("name", key)
        .attr("id", key);
    if (rs->isAutoCalibrationSupported()) {
      input.attr("placeholder", "Use 0 for autocalibration");
    }
    input.attr("value", rs->getClosingTimeMs() / 100, 1).finish();
  });

  if (rs->isTiltFunctionEnabled()) {
    rs->generateKey(key, Supla::ConfigTag::FacadeBlindTiltControlTypeTag);
    emitSelectField(key, "Tilt control type", [&]() {
      sender->selectOption(
          0,
          "OFF",
          rs->getTiltControlType() == SUPLA_TILT_CONTROL_TYPE_UNKNOWN);
      sender->selectOption(
          1,
          "Stands in position while tilting",
          rs->getTiltControlType() ==
              SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING);
      sender->selectOption(
          2,
          "Changes position while tilting",
          rs->getTiltControlType() ==
              SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING);
      sender->selectOption(
          3,
          "Tils only when fully closed",
          rs->getTiltControlType() ==
              SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED);
    });

    rs->generateKey(key, Supla::ConfigTag::FacadeBlindTiltingTimeTag);
    emitField(key, "Tilting time (sec.)", [&]() {
      auto input = sender->voidTag("input");
      input.attr("type", "number")
          .attr("min", 0)
          .attr("max", 300)
          .attr("step", 1, 1)
          .attr("placeholder", "Use 0 for default")
          .attr("name", key)
          .attr("id", key)
          .attr("value", static_cast<int>(rs->getTiltingTimeMs() / 100), 1)
          .finish();
    });
  }

  if (dynamicVisibilityFromChannelFunction) {
    sender->send("</div>");
    sendDynamicVisibilityScript(sender, dynamicContainerId, dynamicFunctionKey);
  }
}

bool RollerShutterParameters::handleResponse(const char* key,
                                             const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (rs == nullptr || rs->getChannel() == nullptr || cfg == nullptr) {
    return false;
  }
  if (showOnlyForRollerFunction &&
      !dynamicVisibilityFromChannelFunction &&
      !isRollerShutterFunction(rs->getChannel()->getDefaultFunction())) {
    return false;
  }

  char keyMatch[16] = {};
  rs->generateKey(keyMatch, Supla::ConfigTag::ChannelFunctionTag);

  // channel function
  if (showChannelFunction && strcmp(key, keyMatch) == 0) {
    int32_t channelFunc = stringToUInt(value);
    if (rs->isFunctionSupported(channelFunc)) {
      rs->setAndSaveFunction(channelFunc);
    } else {
      SUPLA_LOG_WARNING("RsHtml: Unsupported channel function: %d",
                        channelFunc);
      return true;
    }
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::RollerShutterMotorUpsideDownTag);
  if (strcmp(key, keyMatch) == 0) {
    int32_t enabled = stringToUInt(value);
    rs->setRsConfigMotorUpsideDownValue(enabled == 1 ? 2 : 1);
    return true;
  }

  rs->generateKey(keyMatch,
                  Supla::ConfigTag::RollerShutterButtonsUpsideDownTag);
  if (strcmp(key, keyMatch) == 0) {
    int32_t enabled = stringToUInt(value);
    rs->setRsConfigButtonsUpsideDownValue(enabled == 1 ? 2 : 1);
    return true;
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::RollerShutterTimeMarginTag);
  if (strcmp(key, keyMatch) == 0) {
    int32_t timeMargin = stringToInt(value);
    rs->setRsConfigTimeMarginValue(timeMargin);
    return true;
  }

  // open close time
  rs->generateKey(keyMatch, Supla::ConfigTag::RollerShutterOpeningTimeTag);
  if (strcmp(key, keyMatch) == 0) {
    uint32_t time = 0;
    if (!parseTimeMs(value, &time)) {
      pendingFacadeBlindTimingInvalid = true;
    } else {
      if (!pendingFacadeBlindTiming) {
        pendingOpeningTimeMs = rs->getOpeningTimeMs();
        pendingClosingTimeMs = rs->getClosingTimeMs();
        pendingTiltingTimeMs = rs->getTiltingTimeMs();
        pendingTiltControlType = rs->getTiltControlType();
      }
      pendingFacadeBlindTiming = true;
      pendingOpeningTimeMs = time;
    }
    return true;
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::RollerShutterClosingTimeTag);
  if (strcmp(key, keyMatch) == 0) {
    uint32_t time = 0;
    if (!parseTimeMs(value, &time)) {
      pendingFacadeBlindTimingInvalid = true;
    } else {
      if (!pendingFacadeBlindTiming) {
        pendingOpeningTimeMs = rs->getOpeningTimeMs();
        pendingClosingTimeMs = rs->getClosingTimeMs();
        pendingTiltingTimeMs = rs->getTiltingTimeMs();
        pendingTiltControlType = rs->getTiltControlType();
      }
      pendingFacadeBlindTiming = true;
      pendingClosingTimeMs = time;
    }
    return true;
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::FacadeBlindTiltingTimeTag);
  if (strcmp(key, keyMatch) == 0) {
    uint32_t time = 0;
    if (!parseTimeMs(value, &time)) {
      pendingFacadeBlindTimingInvalid = true;
    } else {
      if (!pendingFacadeBlindTiming) {
        pendingOpeningTimeMs = rs->getOpeningTimeMs();
        pendingClosingTimeMs = rs->getClosingTimeMs();
        pendingTiltingTimeMs = rs->getTiltingTimeMs();
        pendingTiltControlType = rs->getTiltControlType();
      }
      pendingFacadeBlindTiming = true;
      pendingTiltingTimeMs = time;
    }
    return true;
  }

  rs->generateKey(keyMatch, Supla::ConfigTag::FacadeBlindTiltControlTypeTag);
  if (strcmp(key, keyMatch) == 0) {
    uint32_t tiltControlType = 0;
    if (!parseUInt32Strict(value, &tiltControlType)) {
      pendingFacadeBlindTimingInvalid = true;
    } else {
      if (!pendingFacadeBlindTiming) {
        pendingOpeningTimeMs = rs->getOpeningTimeMs();
        pendingClosingTimeMs = rs->getClosingTimeMs();
        pendingTiltingTimeMs = rs->getTiltingTimeMs();
        pendingTiltControlType = rs->getTiltControlType();
      }
      pendingFacadeBlindTiming = true;
      pendingTiltControlType = tiltControlType;
    }
    return true;
  }

  return false;
}

void RollerShutterParameters::onProcessingEnd() {
  if ((pendingFacadeBlindTiming || pendingFacadeBlindTimingInvalid) &&
      rs != nullptr) {
    bool applied = !pendingFacadeBlindTimingInvalid;
    if (applied && pendingFacadeBlindTiming) {
      if (rs->isTiltFunctionEnabled()) {
        applied = rs->applyFacadeBlindTimingConfig(pendingOpeningTimeMs,
                                                    pendingClosingTimeMs,
                                                    pendingTiltingTimeMs,
                                                    pendingTiltControlType);
      } else {
        rs->setOpenCloseTime(pendingClosingTimeMs, pendingOpeningTimeMs);
      }
    }
    if (!applied) {
      SUPLA_LOG_WARNING("RsHtml: rejected facade blind timing configuration");
    }
  }

  pendingFacadeBlindTiming = false;
  pendingFacadeBlindTimingInvalid = false;
}

#endif  // ARDUINO_ARCH_AVR
