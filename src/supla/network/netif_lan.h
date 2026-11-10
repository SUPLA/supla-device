// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_NETIF_LAN_H_
#define SRC_SUPLA_NETWORK_NETIF_LAN_H_

#include "network.h"

namespace Supla {
class LAN : public Supla::Network {
 public:
  LAN();
  const char* getIntfName() const override;
  void onLoadConfig() override;
  void fillStateData(TDSC_ChannelState *channelState) override;
};

};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_NETIF_LAN_H_
