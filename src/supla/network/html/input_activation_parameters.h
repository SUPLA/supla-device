// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_INPUT_ACTIVATION_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_INPUT_ACTIVATION_PARAMETERS_H_

#include <supla/device/input_activation_config.h>
#include <supla/network/html_element.h>

namespace Supla {
namespace Html {

class InputActivationParameters : public HtmlElement {
 public:
  explicit InputActivationParameters(
      const Supla::Device::InputActivationProperties &properties);
  virtual ~InputActivationParameters();

  void send(Supla::WebSender *sender) override;
  bool handleResponse(const char *key, const char *value) override;
  void onProcessingEnd() override;

 private:
  void loadConfig();

  Supla::Device::InputActivationProperties properties;
  Supla::Device::InputActivationConfig originalConfig;
  Supla::Device::InputActivationConfig pendingConfig;
  bool configLoaded = false;
  bool modeFound = false;
};

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_INPUT_ACTIVATION_PARAMETERS_H_
