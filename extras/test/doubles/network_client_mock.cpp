// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "network_client_mock.h"
#include "supla/network/client.h"

static NetworkClientMock *networkClientMockPtr = nullptr;

Supla::Client *Supla::ClientBuilder() {
  assert(networkClientMockPtr != nullptr &&
      "please add NetworkClientMock to your test");
  return networkClientMockPtr;
}

NetworkClientMock::NetworkClientMock() {
  assert(networkClientMockPtr == nullptr);
  networkClientMockPtr = this;
}

NetworkClientMock::~NetworkClientMock() {
  networkClientMockPtr = nullptr;
}

