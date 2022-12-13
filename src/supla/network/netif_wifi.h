/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_NETWORK_NETIF_WIFI_H_
#define SRC_SUPLA_NETWORK_NETIF_WIFI_H_

#include "network.h"

#define MAX_SSID_SIZE          33
#define MAX_WIFI_PASSWORD_SIZE 64

namespace Supla {
class Wifi : public Supla::Network {
 public:
  Wifi(const char *wifiSsid = nullptr,
       const char *wifiPassword = nullptr,
       unsigned char *ip = nullptr);

  void setSsid(const char *wifiSsid) override;
  void setPassword(const char *wifiPassword) override;
  bool isWifiConfigRequired() override;

 protected:
  char ssid[MAX_SSID_SIZE] = {};
  char password[MAX_WIFI_PASSWORD_SIZE] = {};
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_NETIF_WIFI_H_
