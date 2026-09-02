// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config_mock.h"

#include <gmock/gmock.h>

ConfigMock::ConfigMock() {
  ON_CALL(*this, isSwUpdateSkipCert()).WillByDefault(::testing::Return(false));
}
ConfigMock::~ConfigMock() {}
