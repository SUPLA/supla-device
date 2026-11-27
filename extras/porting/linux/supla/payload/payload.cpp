// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "payload.h"

#include <string>

#include <nlohmann/json.hpp>

Supla::Payload::Payload::Payload(Supla::Output::Output* out) : output(out) {
}

void Supla::Payload::Payload::addKey(const std::string& key, int index) {
  keys[key] = index;
}

bool Supla::Payload::Payload::publishState(
    const nlohmann::ordered_json &state) {
  if (!isBasedOnIndex()) {
    return output->putContent(state.dump());
  }

  std::string lines;
  auto append = [&lines](const nlohmann::ordered_json &value) {
    if (value.is_boolean()) {
      lines += value.get<bool>() ? "1" : "0";
    } else {
      lines += value.dump();
    }
    lines += '\n';
  };
  for (const auto &field : state) {
    if (field.is_array()) {
      for (const auto &value : field) {
        append(value);
      }
    } else {
      append(field);
    }
  }
  return output->putContent(lines);
}
