// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_CHANNEL_CORRECTION_H_
#define SRC_SUPLA_NETWORK_HTML_CHANNEL_CORRECTION_H_

#include <supla/network/html_element.h>

namespace Supla {

namespace Html {

class ChannelCorrection : public HtmlElement {
 public:
  explicit ChannelCorrection(int channelNumber, const char *displayName,
      int subChannel = 0);
  virtual ~ChannelCorrection();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

 protected:
  int channelNumber = -1;
  int subChannel = -1;
  char *displayName = nullptr;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_CHANNEL_CORRECTION_H_
