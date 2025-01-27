/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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
