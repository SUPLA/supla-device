/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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
