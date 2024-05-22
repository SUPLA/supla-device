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

#include "json.h"

#include <supla/control/custom_relay.h>

#include <string>

Supla::Payload::Json::Json(Supla::Output::Output* out)
    : Supla::Payload::Payload(out) {
}

Supla::Payload::Json::~Json() {
}

bool Supla::Payload::Json::isBasedOnIndex() {
  return false;
}
void Supla::Payload::Json::turnOn(
    const std::string& key, std::variant<int, bool, std::string> onValue) {
  std::visit(
      [this, &key](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        std::string out;
        if constexpr (std::is_same_v<T, int>) {
          out = "{\"" + key + "\": " + std::to_string(arg) + "}";
        } else if constexpr (std::is_same_v<T, bool>) {
          out = "{\"" + key + "\": " + (arg ? "true" : "false") + "}";
        } else if constexpr (std::is_same_v<T, std::string>) {
          out = "{\"" + key + "\": \"" + arg + "\"}";
        }
        output->putContent(out);
      },
      onValue);
}
void Supla::Payload::Json::turnOff(
    const std::string& key, std::variant<int, bool, std::string> offValue) {
  std::visit(
      [this, &key](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        std::string out;
        if constexpr (std::is_same_v<T, int>) {
          out = "{\"" + key + "\": " + std::to_string(arg) + "}";
        } else if constexpr (std::is_same_v<T, bool>) {
          out = "{\"" + key + "\": " + (arg ? "true" : "false") + "}";
        } else if constexpr (std::is_same_v<T, std::string>) {
          out = "{\"" + key + "\": \"" + arg + "\"}";
        }
        output->putContent(out);
      },
      offValue);
}
