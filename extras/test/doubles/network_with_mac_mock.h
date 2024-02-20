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

  void getHostName(char* buffer) {
    memcpy(buffer, hostname, 32);
  }
};

#endif  // EXTRAS_TEST_DOUBLES_NETWORK_WITH_MAC_MOCK_H_
