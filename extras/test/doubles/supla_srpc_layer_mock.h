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

#ifndef EXTRAS_TEST_DOUBLES_SUPLA_SRPC_LAYER_MOCK_H_
#define EXTRAS_TEST_DOUBLES_SUPLA_SRPC_LAYER_MOCK_H_

#include <supla/protocol/supla_srpc.h>
#include <gmock/gmock.h>

class SuplaSrpcLayerMock : public Supla::Protocol::SuplaSrpc {
 public:
  SuplaSrpcLayerMock() : Supla::Protocol::SuplaSrpc(nullptr) {}
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
  MOCK_METHOD(void, sendRegisterNotification, (TDS_RegisterPushNotification *),
              (override));
  MOCK_METHOD(
      bool,
      sendNotification,
      (int context, const char *title, const char *message, int soundId),
      (override));
  MOCK_METHOD(bool, setInitialCaption,
      (uint8_t channelNumber, const char *caption), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_SUPLA_SRPC_LAYER_MOCK_H_
