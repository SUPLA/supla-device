// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_NETWORK_WITH_MAC_MOCK_H_
#define EXTRAS_TEST_DOUBLES_NETWORK_WITH_MAC_MOCK_H_

#include <gmock/gmock.h>
#include <supla/network/network.h>

class NetworkMockWithMac : public Supla::Network {
 public:
  NetworkMockWithMac();
  virtual ~NetworkMockWithMac();
  MOCK_METHOD(void, setup, (), (override));
  MOCK_METHOD(void, disable, (), (override));

  MOCK_METHOD(bool, isReady, (), (override));
  MOCK_METHOD(bool, iterate, (), (override));
  MOCK_METHOD(bool, getMacAddr, (uint8_t*), (override));

  void setWifiState(int8_t rssi, uint8_t signalStrength) {
    wifiStateConfigured = true;
    wifiRssi = rssi;
    wifiSignalStrength = signalStrength;
  }

  void fillStateData(TDSC_ChannelState *channelState) override {
    if (!wifiStateConfigured || channelState == nullptr) {
      return;
    }
    channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_WIFIRSSI |
                            SUPLA_CHANNELSTATE_FIELD_WIFISIGNALSTRENGTH;
    channelState->WiFiRSSI = wifiRssi;
    channelState->WiFiSignalStrength = wifiSignalStrength;
  }

  void getHostName(char* buffer) {
    memcpy(buffer, hostname, 32);
  }

 private:
  bool wifiStateConfigured = false;
  int8_t wifiRssi = 0;
  uint8_t wifiSignalStrength = 0;
};

#endif  // EXTRAS_TEST_DOUBLES_NETWORK_WITH_MAC_MOCK_H_
