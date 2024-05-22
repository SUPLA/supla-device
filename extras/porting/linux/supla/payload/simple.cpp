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
