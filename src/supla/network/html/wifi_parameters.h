// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_WIFI_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_WIFI_PARAMETERS_H_

#include <supla/network/html/network_address_parameters.h>
#include <supla/network/html_element.h>

class SuplaDeviceClass;

namespace Supla {

namespace Html {

class WifiParameters : public HtmlElement {
 public:
  WifiParameters();
  virtual ~WifiParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

 protected:
  void logWifiScanResult();

  bool checkboxFound = false;
  bool wifiSettingsSeen = false;
  char lastLoggedWifiScanMessage[128] = {};
  NetworkAddressParameters netifParameters;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_WIFI_PARAMETERS_H_
