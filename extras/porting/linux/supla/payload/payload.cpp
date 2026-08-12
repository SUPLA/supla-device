// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "payload.h"

#include <string>

Supla::Payload::Payload::Payload(Supla::Output::Output* out) : output(out) {
}

void Supla::Payload::Payload::addKey(const std::string& key, int index) {
  keys[key] = index;
}
