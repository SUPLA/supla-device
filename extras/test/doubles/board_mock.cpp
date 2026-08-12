// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "board_mock.h"

#include <supla/tools.h>
#include <assert.h>
#include <gmock/gmock.h>

namespace {
bool lastResetSoft = false;
bool deviceSoftwareResetSupported = true;
}  // namespace

BoardInterface::BoardInterface() {
  instance = this;
}

BoardInterface::~BoardInterface() {
  instance = nullptr;
}

BoardInterface *BoardInterface::instance = nullptr;


void deviceSoftwareReset() {
  assert(BoardInterface::instance &&
      "Please add BoardMock instance to your test");
  BoardInterface::instance->deviceSoftwareReset();
}

bool isDeviceSoftwareResetSupported() {
  return deviceSoftwareResetSupported;
}

bool isLastResetSoft() {
  return lastResetSoft;
}

bool Supla::isLastResetPower() {
  // TODO(klew): implement
  return false;
}

int Supla::getPlatformId() {
  return 43;
}

BoardMock::BoardMock() {}
BoardMock::~BoardMock() {}

void setLastResetSoft(bool value) {
  lastResetSoft = value;
}

void setDeviceSoftwareResetSupported(bool value) {
  deviceSoftwareResetSupported = value;
}
