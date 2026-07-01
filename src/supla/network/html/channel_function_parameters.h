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

#ifndef SRC_SUPLA_NETWORK_HTML_CHANNEL_FUNCTION_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_CHANNEL_FUNCTION_PARAMETERS_H_

#include <stdint.h>
#include <supla/network/html_element.h>

namespace Supla {
class Channel;
class Element;
class WebSender;

namespace Control {
class RelayRollerShutterPair;
}

namespace Html {

class ChannelFunctionParameters : public HtmlElement {
 public:
  enum class ChannelScope {
    Primary,
    Secondary,
    Both,
  };

  explicit ChannelFunctionParameters(
      Supla::Element *element,
      const char *primaryLabel = nullptr,
      const char *secondaryLabel = nullptr);
  explicit ChannelFunctionParameters(
      Supla::Control::RelayRollerShutterPair *pair,
      const char *primaryLabel = nullptr,
      const char *secondaryLabel = nullptr,
      ChannelScope channelScope = ChannelScope::Both);
  ChannelFunctionParameters(Supla::Control::RelayRollerShutterPair *pair,
                            ChannelScope channelScope);
  virtual ~ChannelFunctionParameters();

  void send(Supla::WebSender *sender) override;
  bool handleResponse(const char *key, const char *value) override;

  static void renderSelectField(Supla::WebSender *sender,
                                Supla::Channel *channel,
                                const char *key,
                                const char *label);

 private:
  static bool isKnownSelectableFunction(uint32_t function);
  static bool renderOptions(Supla::WebSender *sender,
                            Supla::Channel *channel);

  bool handleSingleChannelResponse(const char *key,
                                   const char *value,
                                   Supla::Channel *channel);
  bool handlePairResponse(const char *key, const char *value);

  Supla::Element *element = nullptr;
  Supla::Control::RelayRollerShutterPair *pair = nullptr;
  const char *primaryLabel = nullptr;
  const char *secondaryLabel = nullptr;
  ChannelScope channelScope = ChannelScope::Both;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_CHANNEL_FUNCTION_PARAMETERS_H_
