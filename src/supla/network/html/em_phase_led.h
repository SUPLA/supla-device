// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_EM_PHASE_LED_H_
#define SRC_SUPLA_NETWORK_HTML_EM_PHASE_LED_H_

#include <supla/network/html_element.h>

namespace Supla {
namespace Sensor {
class ElectricityMeter;
}  // namespace Sensor

namespace Html {

class EmPhaseLedParameters : public HtmlElement {
 public:
  explicit EmPhaseLedParameters(Supla::Sensor::ElectricityMeter *em);
  virtual ~EmPhaseLedParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

 private:
  Supla::Sensor::ElectricityMeter *em = nullptr;
  bool channelConfigChanged = false;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_EM_PHASE_LED_H_
