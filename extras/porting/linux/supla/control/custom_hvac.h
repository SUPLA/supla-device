// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_HVAC_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_HVAC_H_

#include <supla/control/hvac_base.h>
#include <supla/payload/payload.h>
#include <supla/control/control_payload.h>

#include <string>

namespace Supla {
namespace Payload {
const char HvacState[] = "set_state";
const char HvacTurnOnPayload[] = "turn_on_payload";
const char HvacTurnOffPayload[] = "turn_off_payload";
};  // namespace Payload

namespace Control {

class CustomOutput;

class CustomHvac : public HvacBase {
 public:
  explicit CustomHvac(Supla::Payload::Payload *payload);
  virtual ~CustomHvac();

  void setMapping(const std::string &parameter, const std::string &key);
  void setMapping(const std::string &parameter, const int index);

  void setSetOnValue(const std::variant<int, bool, std::string>& value);
  void setSetOffValue(const std::variant<int, bool, std::string>& value);
 protected:
  CustomOutput *customOutput = nullptr;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_HVAC_H_
