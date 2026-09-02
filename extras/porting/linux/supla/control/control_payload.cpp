// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "control_payload.h"

#include <string>

Supla::Payload::ControlPayloadBase::ControlPayloadBase(
    Supla::Payload::Payload* payload)
    : payload(payload) {
  static int instanceCounter = 0;
  id = instanceCounter++;
}

void Supla::Payload::ControlPayloadBase::setMapping(
    const std::string& parameter, const std::string& key) {
  parameter2Key[parameter] = key;
  payload->addKey(key, -1);  // ignore index
}

void Supla::Payload::ControlPayloadBase::setMapping(
    const std::string& parameter, const int index) {
  std::string key = parameter;
  key += "_";
  key += std::to_string(id);
  parameter2Key[parameter] = key;
  payload->addKey(key, index);
}

void Supla::Payload::ControlPayloadBase::setSetOnValue(
    const std::variant<int, bool, std::string>& value) {
  setOnValue = value;
}

void Supla::Payload::ControlPayloadBase::setSetOffValue(
    const std::variant<int, bool, std::string>& value) {
  setOffValue = value;
}

