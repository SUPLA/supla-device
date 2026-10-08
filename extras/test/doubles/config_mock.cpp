// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config_mock.h"

#include <gmock/gmock.h>
#include <supla-common/proto.h>

ConfigMock::ConfigMock() {
  ON_CALL(*this, getBlobSize(::testing::_))
      .WillByDefault(::testing::Return(-1));
  ON_CALL(*this, getBlobSize(::testing::EndsWith("_hvac_cfg")))
      .WillByDefault(::testing::Return(sizeof(TChannelConfig_HVAC)));
  ON_CALL(*this, isSwUpdateSkipCert()).WillByDefault(::testing::Return(false));
}
ConfigMock::~ConfigMock() {}
