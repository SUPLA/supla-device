// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "channel_correction.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/element.h>

#include <stdio.h>

namespace Supla {

namespace Html {

ChannelCorrection::ChannelCorrection(int channelNumber, const char *displayName,
    int subChannel) :
  HtmlElement(HTML_SECTION_FORM),
  channelNumber(channelNumber),
  subChannel(subChannel) {
    int size = strlen(displayName);
    this->displayName = new char[size + 1];
    if (this->displayName) {
      snprintf(this->displayName, size + 1, "%s", displayName);
    }
}

ChannelCorrection::~ChannelCorrection() {
  if (displayName) {
    delete[] displayName;
    displayName = nullptr;
  }
}

void ChannelCorrection::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;
    char key[16] = {};
    snprintf(key, sizeof(key), "corr_%d_%d", channelNumber, subChannel);
    cfg->getInt32(key, &value);

    char label[100] = {};
    snprintf(label, sizeof(label), "#%d%s%s correction",
        channelNumber, displayName ? " " : "", displayName ? displayName : "");

    sender->formField([&]() {
      sender->labelFor(key, label);

      sender->voidTag("input")
          .attr("type", "number")
          .attr("min", -50)
          .attr("max", 50)
          .attr("step", 1, 1)
          .attr("name", key)
          .attr("id", key)
          .attr("value", value, 1)
          .finish();
    });
  }
}

bool ChannelCorrection::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  char keyRef[16] = {};
  snprintf(keyRef, sizeof(keyRef), "corr_%d_%d", channelNumber, subChannel);
  if (strcmp(key, keyRef) == 0) {
    int32_t correction = floatStringToInt(value, 1);
    if (correction >= -500 && correction <= 500) {
      int32_t currentValue = 0;
      cfg->getInt32(keyRef, &currentValue);

      if (currentValue != correction) {
        cfg->setInt32(keyRef, correction);

        cfg->setChannelConfigChangeFlag(channelNumber);
      }
    }
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR
