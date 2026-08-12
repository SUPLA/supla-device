// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "custom_hvac.h"

#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/control/output_interface.h>
#include <supla/control/control_payload.h>

#include <cstdio>
#include <string>

using Supla::Control::CustomHvac;

namespace Supla {
namespace Control {
class CustomOutput
: public OutputInterface, public Payload::ControlPayloadBase {
 public:
  explicit CustomOutput(Payload::Payload *payload);

  int getOutputValue() const override;
  void setOutputValue(int value) override;
  bool isOnOffOnly() const override;

 private:
  int lastState = 0;
};
}  // namespace Control
}  // namespace Supla

using Supla::Control::CustomOutput;

CustomOutput::CustomOutput(Payload::Payload* payload)
    : Payload::ControlPayloadBase(payload) {
}

int CustomOutput::getOutputValue() const {
  return lastState;
}

void CustomOutput::setOutputValue(int value) {
  lastState = value;
  if (value == 1) {
    payload->turnOn(parameter2Key[Supla::Payload::HvacState], setOnValue);
  } else if (value == 0) {
    payload->turnOff(parameter2Key[Supla::Payload::HvacState], setOffValue);
  }
}

bool CustomOutput::isOnOffOnly() const {
  return true;
}

CustomHvac::CustomHvac(Payload::Payload *payload) {
  customOutput = new CustomOutput(payload);
  addPrimaryOutput(customOutput);
}

void CustomHvac::setSetOnValue(
    const std::variant<int, bool, std::string>& value) {
  customOutput->setSetOnValue(value);
}

void CustomHvac::setSetOffValue(
    const std::variant<int, bool, std::string>& value) {
  customOutput->setSetOffValue(value);
}

void CustomHvac::setMapping(
    const std::string& parameter, const std::string& key) {
  customOutput->setMapping(parameter, key);
}

void CustomHvac::setMapping(
    const std::string& parameter, const int index) {
  customOutput->setMapping(parameter, index);
}

CustomHvac::~CustomHvac() {}
