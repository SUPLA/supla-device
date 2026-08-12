// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_TIME_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_TIME_PARAMETERS_H_

#include <supla/network/html_element.h>

class SuplaDeviceClass;

namespace Supla {

namespace Html {

class TimeParameters : public HtmlElement {
 public:
  explicit TimeParameters(SuplaDeviceClass* sdc);
  virtual ~TimeParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;
 private:
  SuplaDeviceClass* sdc = nullptr;
  bool processingStarted = false;
  bool checkboxFound = false;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_TIME_PARAMETERS_H_
