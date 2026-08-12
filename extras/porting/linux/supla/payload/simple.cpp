// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "simple.h"

#include <string>

Supla::Payload::Simple::Simple(Supla::Output::Output *out)
    : Supla::Payload::Payload(out) {
}

Supla::Payload::Simple::~Simple() {
}
bool Supla::Payload::Simple::isBasedOnIndex() {
  return true;
}

void Supla::Payload::Simple::turnOn(
    const std::string &key, std::variant<int, bool, std::string> onValue) {
  (void)key;
  std::visit(
      [this](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>)
          output->putContent(arg);
        else if constexpr (std::is_same_v<T, bool>)
          output->putContent(static_cast<int>(arg));
        else if constexpr (std::is_same_v<T, std::string>)
          output->putContent(arg);
      },
      onValue);
}
void Supla::Payload::Simple::turnOff(
    const std::string &key, std::variant<int, bool, std::string> offValue) {
  (void)key;
  std::visit(
      [this](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>)
          output->putContent(arg);
        else if constexpr (std::is_same_v<T, bool>)
          output->putContent(static_cast<int>(arg));
        else if constexpr (std::is_same_v<T, std::string>)
          output->putContent(arg);
      },
      offValue);
}
