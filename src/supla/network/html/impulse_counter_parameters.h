// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_IMPULSE_COUNTER_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_IMPULSE_COUNTER_PARAMETERS_H_

#include <supla/network/html_element.h>

namespace Supla {
namespace Sensor {
class VirtualImpulseCounter;
}  // namespace Sensor

namespace Html {

class ImpulseCounterParameters : public HtmlElement {
 public:
  explicit ImpulseCounterParameters(Sensor::VirtualImpulseCounter *counter,
                                    const char *label = nullptr);
  void send(WebSender *sender) override;
  bool handleResponse(const char *key, const char *value) override;

 private:
  Sensor::VirtualImpulseCounter *counter;
  const char *label;
};

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_IMPULSE_COUNTER_PARAMETERS_H_
