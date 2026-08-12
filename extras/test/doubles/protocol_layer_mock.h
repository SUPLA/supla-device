// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_PROTOCOL_LAYER_MOCK_H_
#define EXTRAS_TEST_DOUBLES_PROTOCOL_LAYER_MOCK_H_

#include <supla/protocol/protocol_layer.h>
#include <gmock/gmock.h>

class ProtocolLayerMock : public Supla::Protocol::ProtocolLayer {
 public:
  ProtocolLayerMock() : Supla::Protocol::ProtocolLayer(nullptr) {}
  MOCK_METHOD(void, onInit, (), (override));
  MOCK_METHOD(bool, onLoadConfig, (), (override));
  MOCK_METHOD(bool, verifyConfig, (), (override));
  MOCK_METHOD(bool, isEnabled, (), (override));
  MOCK_METHOD(void, disconnect, (), (override));
  MOCK_METHOD(bool, isConfigEmpty, (), (override));
  MOCK_METHOD(bool, iterate, (uint32_t _millis), (override));
  MOCK_METHOD(bool, isNetworkRestartRequested, (), (override));
  MOCK_METHOD(uint32_t, getConnectionFailTime, (), (override));
  MOCK_METHOD(bool, isConnectionError, (), (override));
  MOCK_METHOD(bool, isConnecting, (), (override));
  MOCK_METHOD(bool, isUpdatePending, (), (override));
  MOCK_METHOD(bool, isRegisteredAndReady, (), (override));
  MOCK_METHOD(void,
              sendActionTrigger,
              (uint8_t channelNumber, uint32_t actionId),
              (override));
  MOCK_METHOD(void, getUserLocaltime, (), (override));
  MOCK_METHOD(void,
              sendChannelValueChanged,
              (uint8_t channelNumber,
               int8_t *value,
               unsigned char offline,
               uint32_t validityTimeSec),
              (override));
  MOCK_METHOD(void,
              sendExtendedChannelValueChanged,
              (uint8_t channelNumber, TSuplaChannelExtendedValue *value),
              (override));
  MOCK_METHOD(void,
              getChannelConfig,
              (uint8_t channelNumber,
               uint8_t configType),
              (override));
  MOCK_METHOD(bool,
              setChannelConfig,
              (uint8_t channelNumber,
               _supla_int_t channelFunction,
               void *channelConfig,
               int size,
               uint8_t configType),
              (override));
  MOCK_METHOD(bool,
              setDeviceConfig,
              (TSDS_SetDeviceConfig * deviceConfig),
              (override));
  MOCK_METHOD(
      void,
      sendRemainingTimeValue,
      (uint8_t channelNumber, uint32_t timeMs, uint8_t state, int32_t senderId),
      (override));
};

#endif  // EXTRAS_TEST_DOUBLES_PROTOCOL_LAYER_MOCK_H_
