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
