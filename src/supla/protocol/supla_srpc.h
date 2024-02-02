/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef SRC_SUPLA_PROTOCOL_SUPLA_SRPC_H_
#define SRC_SUPLA_PROTOCOL_SUPLA_SRPC_H_

#include <supla-common/proto.h>

#include "protocol_layer.h"

#define SUPLA_SRPC_CALCFG_RESULT_DONT_REPLY (-1)
#define SUPLA_SRPC_CALCFG_RESULT_PENDING (-2)

namespace Supla {

class Client;

namespace Device {
class RemoteDeviceConfig;
}  // namespace Device

namespace Protocol {

struct CalCfgResultPendingItem;

class CalCfgResultPending {
 public:
  friend class SuplaSrpc;
  void set(uint8_t channelNo, int32_t receiverId, int32_t command);
  void clear(uint8_t channelNo);
  CalCfgResultPendingItem *get(uint8_t channelNo);

 protected:
  CalCfgResultPending();
  ~CalCfgResultPending();
  CalCfgResultPendingItem *first = nullptr;
};

class SuplaSrpc : public ProtocolLayer {
 public:
  explicit SuplaSrpc(SuplaDeviceClass *sdc, int version = 16);
  ~SuplaSrpc();

  static bool isSuplaPublicServerConfigured();

  void onInit() override;
  bool onLoadConfig() override;
  bool verifyConfig() override;
  bool isEnabled() override;
  void disconnect() override;
  bool iterate(uint32_t _millis) override;
  bool isNetworkRestartRequested() override;
  uint32_t getConnectionFailTime() override;
  bool isRegisteredAndReady() override;

  void sendActionTrigger(uint8_t channelNumber, uint32_t actionId) override;
  void sendRegisterNotification(
      TDS_RegisterPushNotification *notification) override;
  bool sendNotification(int context,
                      const char *title,
                      const char *message,
                      int soundId) override;
  void getUserLocaltime() override;
  void sendChannelValueChanged(uint8_t channelNumber, char *value,
      unsigned char offline, uint32_t validityTimeSec) override;
  void sendExtendedChannelValueChanged(uint8_t channelNumber,
    TSuplaChannelExtendedValue *value) override;

  void getChannelConfig(uint8_t channelNumber, uint8_t configType) override;
  bool setChannelConfig(uint8_t channelNumber,
      _supla_int_t channelFunction, void *channelConfig, int size,
      uint8_t configType) override;

  bool setDeviceConfig(TSDS_SetDeviceConfig *deviceConfig) override;
  void sendRemainingTimeValue(uint8_t channelNumber,
                              uint32_t timeMs,
                              uint8_t state,
                              int32_t senderId) override;
  void sendRemainingTimeValue(uint8_t channelNumber,
                              uint32_t remainingTime,
                              unsigned char state[SUPLA_CHANNELVALUE_SIZE],
                              int32_t senderId,
                              bool useSecondsInsteadOfMs) override;

  void onVersionError(TSDC_SuplaVersionError *versionError);
  void onRegisterResult(TSD_SuplaRegisterDeviceResult *register_device_result);
  void onSetActivityTimeoutResult(
      TSDC_SuplaSetActivityTimeoutResult *result);
  void setActivityTimeout(uint32_t activityTimeoutSec);
  uint32_t getActivityTimeout();
  void updateLastResponseTime();
  void updateLastSentTime();
  void onGetUserLocaltimeResult(TSDC_UserLocalTimeResult *result);
  void sendChannelStateResult(int32_t receiverId, uint8_t channelNo);

  void setServerPort(int value);
  void setVersion(int value);
  void setSuplaCACert(const char *);
  void setSupla3rdPartyCACert(const char *);
  const char* getSuplaCACert();
  const char* getSupla3rdPartyCACert();
  bool isUpdatePending() override;
  void handleDeviceConfig(TSDS_SetDeviceConfig *deviceConfig);
  void handleSetDeviceConfigResult(TSDS_SetDeviceConfigResult *result);

  Supla::Client *client = nullptr;
  CalCfgResultPending calCfgResultPending;
  void sendPendingCalCfgResult(uint8_t channelNo,
                               int32_t result,
                               int32_t command,
                               int dataSize = 0,
                               void *data = nullptr);

  static const char *configResultToCStr(int result);

 protected:
  bool ping();
  void initializeSrpc();
  void deinitializeSrpc();

  uint8_t version = 0;
  uint8_t activityTimeoutS = 30;
  uint8_t securityLevel = 0;
  int8_t registered = 0;
  bool requestNetworkRestart = false;
  bool enabled = true;
  bool setDeviceConfigReceivedAfterRegistration = false;
  bool firstConnectionAttempt = true;
  uint16_t connectionFailCounter = 0;

  uint32_t lastPingTimeMs = 0;
  uint32_t waitForIterate = 0;
  uint32_t lastIterateTime = 0;
  uint32_t lastResponseMs = 0;
  uint32_t lastSentMs = 0;

  int port = -1;

  const char *suplaCACert = nullptr;
  const char *supla3rdPartyCACert = nullptr;
  void *srpc = nullptr;

 private:
  Supla::Device::RemoteDeviceConfig *remoteDeviceConfig = nullptr;
};
}  // namespace Protocol

// Method passed to SRPC as a callback to read raw data from network interface
_supla_int_t dataRead(void *buf, _supla_int_t count, void *sdc);
// Method passed to SRPC as a callback to write raw data to network interface
_supla_int_t dataWrite(void *buf, _supla_int_t count, void *sdc);
// Method passed to SRPC as a callback to handle response from Supla server
void messageReceived(void *srpc,
                      unsigned _supla_int_t rrId,
                      unsigned _supla_int_t callType,
                      void *userParam,
                      unsigned char protoVersion);

}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_SUPLA_SRPC_H_
