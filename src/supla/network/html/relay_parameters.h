// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_RELAY_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_RELAY_PARAMETERS_H_

#include <stdint.h>
#include <supla/network/html_element.h>

namespace Supla {
class WebSender;

namespace Control {
class Relay;
}  // namespace Control

namespace Html {

class RelayParameters : public HtmlElement {
 public:
  explicit RelayParameters(Supla::Control::Relay *relay);
  virtual ~RelayParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

  // When enabled, update the visibility of the turn-on duration input as the
  // channel function select field changes in local WWW.
  void setDynamicTimeVisibilityFromChannelFunction(bool enabled);

 private:
  static bool isTimedFunction(uint32_t function);

  Supla::Control::Relay *relay = nullptr;
  uint32_t pendingTurnOnDurationMs = 0;
  bool turnOnDurationSeen = false;
  bool dynamicTimeVisibilityFromChannelFunction = false;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_RELAY_PARAMETERS_H_
