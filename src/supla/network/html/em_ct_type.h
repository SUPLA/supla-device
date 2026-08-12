// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
