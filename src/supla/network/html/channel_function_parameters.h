// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
