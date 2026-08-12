// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_NETWORK_H_
#define EXTRAS_PORTING_LINUX_LINUX_NETWORK_H_

#include <supla/network/network.h>

namespace Supla {

class LinuxNetwork : public Network {
 public:
  LinuxNetwork();
  ~LinuxNetwork() override;

  bool isReady() override;
  void setup() override;
  bool iterate() override;
  void disable() override;
  void fillStateData(TDSC_ChannelState *channelState) override;

 protected:
  bool isDeviceReady = false;
};

};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_NETWORK_H_
