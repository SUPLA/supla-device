// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_PROTOCOL_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_PROTOCOL_PARAMETERS_H_

#include <supla/network/html_element.h>

class SuplaDeviceClass;

namespace Supla {

namespace Html {

class ProtocolParameters : public HtmlElement {
 public:
  explicit ProtocolParameters(
      bool addMqttParams = true, bool concurrentProtocols = true);
  virtual ~ProtocolParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

 protected:
  bool addMqtt = true;
  bool concurrent = true;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_PROTOCOL_PARAMETERS_H_
