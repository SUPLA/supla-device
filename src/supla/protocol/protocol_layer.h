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

#ifndef SRC_SUPLA_PROTOCOL_PROTOCOL_LAYER_H_
#define SRC_SUPLA_PROTOCOL_PROTOCOL_LAYER_H_

#include <stdint.h>
#include <supla-common/proto.h>

class SuplaDeviceClass;

namespace Supla {

namespace Protocol {

class ProtocolLayer {
 public:
  explicit ProtocolLayer(SuplaDeviceClass *sdc);
  virtual ~ProtocolLayer();
  static ProtocolLayer *first();
  static ProtocolLayer *last();
  static bool IsAnyUpdatePending();
  ProtocolLayer *next();
  SuplaDeviceClass *getSdc();

  virtual void onInit() = 0;
  virtual bool onLoadConfig() = 0;
  virtual bool verifyConfig() = 0;
  virtual bool isEnabled() = 0;
  virtual void disconnect() = 0;
  virtual bool isConfigEmpty();
  // Return value indicates if specific protocol is ready to handle data
  // from other elements and if call to Element::iterateConnected should be
  // done.
  virtual bool iterate(uint32_t _millis) = 0;
  virtual bool isNetworkRestartRequested() = 0;
  virtual uint32_t getConnectionFailTime() = 0;
  virtual bool isConnectionError();
  virtual bool isConnecting();
  virtual bool isUpdatePending();
  virtual bool isRegisteredAndReady() = 0;
  virtual void sendActionTrigger(uint8_t channelNumber, uint32_t actionId) = 0;
  virtual void sendRemainingTimeValue(uint8_t channelNumber,
                                      uint32_t timeMs,
                                      uint8_t state,
                                      int32_t senderId);
  virtual void sendRemainingTimeValue(uint8_t channelNumber,
                                      uint32_t remainingTime,
                                      uint8_t* state,
                                      int32_t senderId,
                                      bool useSecondsInsteadOfMs);
  virtual void getUserLocaltime();
  virtual void sendChannelValueChanged(uint8_t channelNumber, char *value,
      unsigned char offline, uint32_t validityTimeSec) = 0;
  virtual void sendExtendedChannelValueChanged(uint8_t channelNumber,
    TSuplaChannelExtendedValue *value) = 0;

  virtual void getChannelConfig(uint8_t channelNumber,
      uint8_t configType = SUPLA_CONFIG_TYPE_DEFAULT);
  virtual bool setChannelConfig(uint8_t channelNumber,
      _supla_int_t channelFunction, void *channelConfig, int size,
      uint8_t configType = SUPLA_CONFIG_TYPE_DEFAULT);
  virtual void notifyConfigChange(int channelNumber);

  virtual bool setDeviceConfig(TSDS_SetDeviceConfig *deviceConfig);

  virtual void sendRegisterNotification(
      TDS_RegisterPushNotification *notification);
  virtual bool sendNotification(int context,
                              const char *title,
                              const char *message,
                              int soundId);

 protected:
  static ProtocolLayer *firstPtr;
  ProtocolLayer *nextPtr = nullptr;
  SuplaDeviceClass *sdc = nullptr;
  bool configEmpty = true;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_PROTOCOL_LAYER_H_
