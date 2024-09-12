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

#ifndef SRC_SUPLA_NETWORK_HTML_EM_CT_TYPE_H_
#define SRC_SUPLA_NETWORK_HTML_EM_CT_TYPE_H_

#include <supla/network/html/select_input_parameter.h>

namespace Supla {
namespace Sensor {
class ElectricityMeter;
}  // namespace Sensor

namespace Html {

class EmCtTypeParameters : public SelectInputParameter {
 public:
  explicit EmCtTypeParameters(Supla::Sensor::ElectricityMeter *em);
  void onProcessingEnd() override;

 private:
  Supla::Sensor::ElectricityMeter *em = nullptr;
  bool channelConfigChanged = false;
};

};  // namespace Html
};  // namespace Supla



#endif  // SRC_SUPLA_NETWORK_HTML_EM_CT_TYPE_H_
