// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_NETWORK_CLIENT_MOCK_H_
#define EXTRAS_TEST_DOUBLES_NETWORK_CLIENT_MOCK_H_

#include <gmock/gmock.h>

#include <supla/network/client.h>
#include <SuplaDevice.h>

class NetworkClientMock : public Supla::Client {
 public:
  NetworkClientMock();
  virtual ~NetworkClientMock();

  MOCK_METHOD(int, available, (), (override));
  MOCK_METHOD(void, stop, (), (override));
  MOCK_METHOD(uint8_t, connected, (), (override));
  MOCK_METHOD(void, setTimeoutMs, (uint16_t timeoutMs), (override));
  MOCK_METHOD(int, connectImp, (const char *host, uint16_t port), (override));
  MOCK_METHOD(size_t, writeImp, (const uint8_t *buf, size_t size), (override));
  MOCK_METHOD(int, readImp, (uint8_t * buf, size_t size), (override));

  const char *getRootCACert() {
    return rootCACert;
  }
};

#endif  // EXTRAS_TEST_DOUBLES_NETWORK_CLIENT_MOCK_H_
