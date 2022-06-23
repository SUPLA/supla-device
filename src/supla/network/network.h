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

#include "supla-common/log.h"
#include "supla-common/proto.h"
#include "supla/storage/config.h"

class SuplaDeviceClass;

namespace Supla {
class Network {
 public:
  static Network *Instance();
  static bool Connected();
  static int Read(void *buf, int count);
  static int Write(void *buf, int count);
  static int Connect(const char *server, int port = -1);
  static void Disconnect();
  static void Setup();
  static void Uninit();
  static bool IsReady();
  static bool Iterate();
  static bool Ping(void *srpc);
  static void SetConfigMode();
  static void SetNormalMode();
  static bool GetMacAddr(uint8_t *);
  static void SetHostname(const char *);

  static void printData(const char *prefix, const void *buf, const int count);

  explicit Network(uint8_t ip[4]);
  virtual ~Network();
  virtual int read(void *buf, int count) = 0;
  virtual int write(void *buf, int count) = 0;
  virtual int connect(const char *server, int port = -1) = 0;
  virtual bool connected() = 0;
  virtual void disconnect() = 0;
  virtual void setup() = 0;
  virtual void uninit();
  virtual void setTimeout(int);
  virtual void setConfigMode();
  virtual void setNormalMode();
  virtual bool getMacAddr(uint8_t *);
  virtual void setHostname(const char *);

  virtual bool isReady() = 0;
  virtual bool iterate();
  virtual bool ping(void *);

  virtual void fillStateData(TDSC_ChannelState *channelState);

  // WiFi specific part
  virtual bool isWifiConfigRequired();
  virtual void setSsid(const char *wifiSsid);
  virtual void setPassword(const char *wifiPassword);

  // SSL configuration
  virtual void setSSLEnabled(bool enabled);
  void setCACert(const char *rootCA);

  void updateLastSent();
  void updateLastResponse();
  void clearTimeCounters();
  void setActivityTimeout(_supla_int_t activityTimeoutSec);
  void setSuplaDeviceClass(SuplaDeviceClass *);

 protected:
  static Network *netIntf;
  _supla_int64_t lastSentMs;
  _supla_int64_t lastResponseMs;
  _supla_int64_t lastPingTimeMs;
  _supla_int_t serverActivityTimeoutS;
  void *srpc;
  SuplaDeviceClass *sdc = nullptr;

  enum DeviceMode mode = DEVICE_MODE_NORMAL;
  bool modeChanged = false;
  bool useLocalIp;
  unsigned char localIp[4];
  char hostname[32] = {};

  bool sslEnabled = true;
  unsigned char *rootCACert = nullptr;
  unsigned int rootCACertSize = 0;
};

// Method passed to SRPC as a callback to read raw data from network interface
_supla_int_t data_read(void *buf, _supla_int_t count, void *sdc);
// Method passed to SRPC as a callback to write raw data to network interface
_supla_int_t data_write(void *buf, _supla_int_t count, void *sdc);
// Method passed to SRPC as a callback to handle response from Supla server
void message_received(void *_srpc,
                      unsigned _supla_int_t rr_id,
                      unsigned _supla_int_t call_type,
                      void *_sdc,
                      unsigned char proto_version);

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_NETWORK_H_
