/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

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
