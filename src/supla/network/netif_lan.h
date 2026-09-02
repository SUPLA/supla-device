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
  uint32_t getIP() override;
  bool isReady() override;
  void onLoadConfig() override;
  void fillStateData(TDSC_ChannelState *channelState) override;
  void setIpReady(bool ready);
  void setIpv4Addr(uint32_t ip);

 protected:
  bool isIpReady = false;
  uint32_t ipv4 = 0;
};

};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_NETIF_LAN_H_
