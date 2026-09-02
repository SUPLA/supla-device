// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_NETWORK_MOCK_H_
#define EXTRAS_TEST_DOUBLES_NETWORK_MOCK_H_

#include <gmock/gmock.h>
#include <supla/network/network.h>

class NetworkMock : public Supla::Network {
 public:
  NetworkMock();
  virtual ~NetworkMock();
  MOCK_METHOD(void, setup, (), (override));
  MOCK_METHOD(void, disable, (), (override));

  MOCK_METHOD(bool, isReady, (), (override));
  MOCK_METHOD(bool, iterate, (), (override));
  MOCK_METHOD(bool, isWifiConfigRequired, (), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_NETWORK_MOCK_H_
