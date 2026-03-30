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

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/storage/config_tags.h>

#include "button_multiclick_parameters.h"

namespace Supla {

namespace Html {

ButtonMulticlickParameters::ButtonMulticlickParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

ButtonMulticlickParameters::~ButtonMulticlickParameters() {
}

void ButtonMulticlickParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint32_t value = 300;  // default value
    cfg->getUInt32(Supla::ConfigTag::BtnMulticlickTag, &value);
    if (value < 200) {
      value = 200;
    } else if (value > 10000) {
      value = 10000;
    }

    sender->labeledField(
        Supla::ConfigTag::BtnMulticlickTag,
        "Multiclick detection time [s]",
        [&]() {
          sender->numberInput(
              Supla::ConfigTag::BtnMulticlickTag,
              {
                  .min = fixed(200, 3),
                  .max = fixed(10000, 3),
                  .value = fixed(static_cast<int>(value), 3),
                  .step = fixed(100, 3),
              });
        });
  }
}

bool ButtonMulticlickParameters::handleResponse(const char* key,
                                                const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, Supla::ConfigTag::BtnMulticlickTag) == 0) {
    uint32_t param = floatStringToInt(value, 3);
    if (param >= 200 && param <= 10000) {
      cfg->setUInt32(Supla::ConfigTag::BtnMulticlickTag, param);
    }
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla
