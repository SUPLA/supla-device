// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_THERMAL_PROTECTION_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_THERMAL_PROTECTION_PARAMETERS_H_

#include <supla/device/thermal_protection_config.h>
#include <supla/network/html_element.h>

namespace Supla {
namespace Html {

class ThermalProtectionParameters : public HtmlElement {
 public:
  explicit ThermalProtectionParameters(
      const Supla::Device::ThermalProtectionProperties &properties);
  virtual ~ThermalProtectionParameters();

  void send(Supla::WebSender *sender) override;
  bool handleResponse(const char *key, const char *value) override;
  void onProcessingEnd() override;

 private:
  void loadConfig();

  Supla::Device::ThermalProtectionProperties properties;
  Supla::Device::ThermalProtectionConfig originalConfig;
  Supla::Device::ThermalProtectionConfig pendingConfig;
  bool configLoaded = false;
  bool thresholdFound = false;
  bool enabledFound = false;
};

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_THERMAL_PROTECTION_PARAMETERS_H_
