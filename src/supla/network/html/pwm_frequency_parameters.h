// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_PWM_FREQUENCY_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_PWM_FREQUENCY_PARAMETERS_H_

#include <supla/network/html_element.h>

namespace Supla {
class WebSender;

namespace Control {
class LightingPwmBase;
}  // namespace Control

namespace Html {

class PwmFrequencyParameters : public HtmlElement {
 public:
  explicit PwmFrequencyParameters(Supla::Control::LightingPwmBase *rgbCct);
  virtual ~PwmFrequencyParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

 private:
  Supla::Control::LightingPwmBase *rgbCct = nullptr;
};

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_PWM_FREQUENCY_PARAMETERS_H_
