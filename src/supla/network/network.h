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

#ifndef SRC_SUPLA_NETWORK_NETWORK_H_
#define SRC_SUPLA_NETWORK_NETWORK_H_

#include <stdint.h>

#include "supla-common/proto.h"
#include "supla/storage/config.h"

class SuplaDeviceClass;

namespace Supla {
class Network {
 public:
  static Network *Instance();
  static void DisconnectProtocols();
  static void Setup();
  static void Disable();
  static void Uninit();
  static bool IsReady();
  static bool Iterate();
  static void SetConfigMode();
  static void SetNormalMode();
  static void SetSetupNeeded();
  static bool PopSetupNeeded();
  static bool GetMacAddr(uint8_t *);
  static void SetHostname(const char *);
  static bool IsSuplaSSLEnabled();

  static void printData(const char *prefix, const void *buf, const int count);

  explicit Network(uint8_t ip[4]);
  virtual ~Network();
  virtual void setup() = 0;
  virtual void disable() = 0;
  virtual void uninit();
  virtual void setConfigMode();
  virtual void setNormalMode();
  virtual bool getMacAddr(uint8_t *);
  virtual void setHostname(const char *);
  virtual bool isSuplaSSLEnabled();

  virtual bool isReady() = 0;
  virtual bool iterate();

  virtual void fillStateData(TDSC_ChannelState *channelState);

  // WiFi specific part
  virtual bool isWifiConfigRequired();
  virtual void setSsid(const char *wifiSsid);
  virtual void setPassword(const char *wifiPassword);

  // SSL configuration
  virtual void setSSLEnabled(bool enabled);
  bool isSSLEnabled();
  void setCACert(const char *rootCA);

  void clearTimeCounters();
  void setSuplaDeviceClass(SuplaDeviceClass *);

  void setSetupNeeded();
  bool popSetupNeeded();

 protected:
  static Network *netIntf;
  SuplaDeviceClass *sdc = nullptr;

  enum DeviceMode mode = DEVICE_MODE_NORMAL;
  bool setupNeeded = false;
  bool useLocalIp;
  unsigned char localIp[4];
  char hostname[32] = {};

  bool sslEnabled = true;
  const char *rootCACert = nullptr;
  unsigned int rootCACertSize = 0;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_NETWORK_H_
