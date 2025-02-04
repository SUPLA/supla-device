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

#include "netif_wifi.h"

#include <string.h>
#include <supla/storage/config.h>

using Supla::Wifi;

Wifi::Wifi(const char *wifiSsid, const char *wifiPassword, unsigned char *ip)
  : Network(ip) {
    setSsid(wifiSsid);
    setPassword(wifiPassword);
    intfType = IntfType::WiFi;
  }

void Wifi::setSsid(const char *wifiSsid) {
  if (wifiSsid) {
    strncpy(ssid, wifiSsid, MAX_SSID_SIZE - 1);
    ssid[MAX_SSID_SIZE - 1] = '\0';
  }
}

void Wifi::setPassword(const char *wifiPassword) {
  if (wifiPassword) {
    strncpy(password, wifiPassword, MAX_WIFI_PASSWORD_SIZE - 1);
    password[MAX_WIFI_PASSWORD_SIZE - 1] = '\0';
  }
}

bool Wifi::isWifiConfigRequired() {
  return true;
}


void Wifi::onLoadConfig() {
  Network::onLoadConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  char buf[100] = {};
  memset(buf, 0, sizeof(buf));
  if (cfg->getWiFiSSID(buf) && strlen(buf) > 0) {
    setSsid(buf);
  }

  memset(buf, 0, sizeof(buf));
  if (cfg->getWiFiPassword(buf) && strlen(buf) > 0) {
    setPassword(buf);
  }
}

const char* Wifi::getIntfName() const {
  return "Wi-Fi";
}
