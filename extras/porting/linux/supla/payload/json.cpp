// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
