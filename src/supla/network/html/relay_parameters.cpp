/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifndef ARDUINO_ARCH_AVR
#include "relay_parameters.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <supla/clock/clock.h>
#include <supla/control/relay.h>
#include <supla/log_wrapper.h>
#include <supla/network/html_element.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

using Supla::Html::RelayParameters;

namespace {

constexpr uint32_t kMaxTurnOnDurationDeciseconds = 36000;

void sendDynamicTimeVisibilityScript(Supla::WebSender *sender,
                                     const char *containerId,
                                     const char *functionSelectId) {
  if (sender == nullptr || containerId == nullptr ||
      functionSelectId == nullptr) {
    return;
  }

  char script[760] = {};
  snprintf(script,
           sizeof(script),
           "<script>"
           "(function(){"
           "function u(){"
           "var s=document.getElementById('%s'),"
           "b=document.getElementById('%s');"
           "if(!s||!b)return;"
           "var v=s.value,on=(v=='%d'||v=='%d'||v=='%d'||v=='%d'||v=='%d');"
           "b.style.display=on?'block':'none';"
           "var i=b.getElementsByTagName('input');"
           "for(var n=0;n<i.length;n++){i[n].disabled=!on;}"
           "s.addEventListener('change',u);"
           "}"
           "if(document.readyState=='loading'){"
           "document.addEventListener('DOMContentLoaded',u);"
           "}else{u();}"
           "})();"
           "</script>",
           functionSelectId,
           containerId,
           SUPLA_CHANNELFNC_STAIRCASETIMER,
           SUPLA_CHANNELFNC_CONTROLLINGTHEGATE,
           SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK,
           SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR,
           SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK);
  sender->send(script);
}

}  // namespace

RelayParameters::RelayParameters(Supla::Control::Relay* relay)
    : HtmlElement(HTML_SECTION_FORM), relay(relay) {
}

RelayParameters::~RelayParameters() {
}

void RelayParameters::setDynamicTimeVisibilityFromChannelFunction(
    bool enabled) {
  dynamicTimeVisibilityFromChannelFunction = enabled;
}

bool RelayParameters::isTimedFunction(uint32_t function) {
  return function == SUPLA_CHANNELFNC_STAIRCASETIMER ||
         function == SUPLA_CHANNELFNC_CONTROLLINGTHEGATE ||
         function == SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK ||
         function == SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR ||
         function == SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK;
}

void RelayParameters::send(Supla::WebSender* sender) {
  if (relay != nullptr && sender != nullptr &&
      relay->getChannelNumber() >= 0) {
    if (relay->getOvercurrentMaxAllowed() > 0) {
      uint32_t value = relay->getOvercurrentThreshold();
      char key[16] = {};
      Supla::Config::generateKey(
          key,
          relay->getChannelNumber(),
          Supla::ConfigTag::RelayOvercurrentThreshold);

      sender->labeledField(key, "Overcurrent protection [A]", [&]() {
        sender->numberInput(
            key,
            {
                .min = 0,
                .max = fixed(
                    static_cast<int>(relay->getOvercurrentMaxAllowed()), 2),
                .value = fixed(static_cast<int>(value), 2),
                .step = fixed(1, 2),
            });
      });
    }

    const bool showTime = dynamicTimeVisibilityFromChannelFunction ||
                          isTimedFunction(
                              relay->getChannel()->getDefaultFunction());
    if (!showTime) {
      return;
    }

    char timeKey[16] = {};
    relay->generateKey(timeKey, Supla::ConfigTag::RelayTurnOnDuration);
    char containerId[32] = {};
    char functionKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    if (dynamicTimeVisibilityFromChannelFunction) {
      snprintf(containerId,
               sizeof(containerId),
               "relay_time_%d",
               relay->getChannelNumber());
      relay->generateKey(functionKey, Supla::ConfigTag::ChannelFunctionTag);
      sender->send("<div id=\"");
      sender->send(containerId);
      sender->send("\" style=\"display:none\">");
    }

    sender->labeledField(timeKey, "Turn-on duration (sec.)", [&]() {
      sender->numberInput(
          timeKey,
          {
              .min = fixed(1, 1),
              .max =
                  fixed(static_cast<int>(kMaxTurnOnDurationDeciseconds), 1),
              .value = fixed(
                  static_cast<int>(relay->getStoredTurnOnDurationMs() / 100),
                  1),
              .step = fixed(1, 1),
          });
    });

    if (dynamicTimeVisibilityFromChannelFunction) {
      sender->send("</div>");
      sendDynamicTimeVisibilityScript(sender, containerId, functionKey);
    }
  }
}

bool RelayParameters::handleResponse(const char* key, const char* value) {
  if (relay == nullptr || key == nullptr || value == nullptr) {
    return false;
  }

  char expectedKey[16] = {};
  Supla::Config::generateKey(expectedKey,
                             relay->getChannelNumber(),
                             Supla::ConfigTag::RelayOvercurrentThreshold);

  if (relay->getOvercurrentMaxAllowed() > 0 &&
      strcmp(key, expectedKey) == 0) {
    uint32_t param = floatStringToInt(value, 2);
    relay->setOvercurrentThreshold(param);
    return true;
  }

  relay->generateKey(expectedKey, Supla::ConfigTag::RelayTurnOnDuration);
  if (strcmp(key, expectedKey) == 0) {
    const int32_t durationDeciseconds = floatStringToInt(value, 1);
    if (durationDeciseconds < 1 ||
        durationDeciseconds >
            static_cast<int32_t>(kMaxTurnOnDurationDeciseconds)) {
      SUPLA_LOG_WARNING("RelayHtml: invalid turn-on duration %s", value);
      return true;
    }
    pendingTurnOnDurationMs =
        static_cast<uint32_t>(durationDeciseconds) * 100;
    turnOnDurationSeen = true;
    return true;
  }

  return false;
}

void RelayParameters::onProcessingEnd() {
  if (!turnOnDurationSeen) {
    return;
  }

  if (relay != nullptr &&
      isTimedFunction(relay->getChannel()->getDefaultFunction())) {
    relay->setStoredTurnOnDurationMs(pendingTurnOnDurationMs);
    Supla::Storage::ScheduleSave(5000, 2000);
  }
  turnOnDurationSeen = false;
}

#endif  // ARDUINO_ARCH_AVR
