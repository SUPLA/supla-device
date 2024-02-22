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

const char WifiDisableTag[] = "wifi_dis";
const char EthDisableTag[] = "eth_dis";

class Client;

class Network {
 public:
  enum class IntfType {
    Unknown = 0,
    Ethernet = 1,
    WiFi = 2,
  };

  static Network *Instance();
  static Network *FirstInstance();
  static Network *NextInstance(Network *instance);
  static Network *GetInstanceByIP(uint32_t ip);
  static int GetNetIntfCount();
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
  // returns MAC addres of the main network interface
  static bool GetMainMacAddr(uint8_t *);
  // Initialize hostname on all network interfaces with given prefix
  static void SetHostname(const char *prefix, int macSize);
  // Returns true when all network interfaces are in timeout
  static bool IsIpSetupTimeout();
  static void LoadConfig();

  static void printData(const char *prefix, const void *buf, const int count);

  explicit Network(uint8_t ip[4]);
  Network();
  virtual ~Network();
  virtual void onLoadConfig();
  virtual void setup() = 0;
  virtual void disable() = 0;
  virtual void uninit();
  virtual void setConfigMode();
  virtual void setNormalMode();
  virtual bool getMacAddr(uint8_t *);
  virtual void setHostname(const char *, int macSize);
  void generateHostname(const char *prefix, int macSize, char *output);
  virtual bool isIpSetupTimeout();
  virtual uint32_t getIP();

  virtual Supla::Client *createClient();

  virtual bool isReady() = 0;
  virtual bool iterate();

  virtual void fillStateData(TDSC_ChannelState *channelState);

  // WiFi specific part
  virtual bool isWifiConfigRequired();
  virtual void setSsid(const char *wifiSsid);
  virtual void setPassword(const char *wifiPassword);

  // SSL configuration
  static void setSSLEnabled(bool enabled);

  void clearTimeCounters();
  void setSuplaDeviceClass(SuplaDeviceClass *);

  void setSetupNeeded();
  bool popSetupNeeded();

  enum IntfType getIntfType() const;
  virtual const char* getIntfName() const;

  bool isIntfDisabledInConfig() const;

 protected:
  static Network *netIntf;
  static Network *firstNetIntf;

  Network *nextNetIntf = nullptr;
  SuplaDeviceClass *sdc = nullptr;
  const char *rootCACert = nullptr;
  unsigned int rootCACertSize = 0;
  unsigned char localIp[4];
  char hostname[32] = {};

  enum IntfType intfType = IntfType::Unknown;

  static enum DeviceMode mode;
  bool setupNeeded = false;
  bool useLocalIp = false;
  bool intfDisabledInConfig = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_NETWORK_H_
