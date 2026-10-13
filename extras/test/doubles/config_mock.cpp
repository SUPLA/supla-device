// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config_mock.h"

#include <gmock/gmock.h>
#include <supla-common/proto.h>
#include <string>

ConfigMock::ConfigMock() {
  ON_CALL(*this, getBlobSize(::testing::_))
      .WillByDefault(::testing::Return(-1));
  ON_CALL(*this, getBlobSize(::testing::EndsWith("_hvac_cfg")))
      .WillByDefault(::testing::Return(sizeof(TChannelConfig_HVAC)));
  ON_CALL(*this, isSwUpdateSkipCert()).WillByDefault(::testing::Return(false));
}
ConfigMock::~ConfigMock() {}

void ConfigMock::expectLegacyHvacBlobSizes(int channelNumber) {
  const auto prefix = std::to_string(channelNumber);
  EXPECT_CALL(*this, getBlobSize(::testing::StrEq(prefix + "_hvac_cfg2")))
      .Times(::testing::AnyNumber())
      .WillRepeatedly(::testing::Return(-1));
  EXPECT_CALL(*this, getBlobSize(::testing::StrEq(prefix + "_hvac_cfg")))
      .Times(::testing::AnyNumber())
      .WillRepeatedly(::testing::Return(sizeof(TChannelConfig_HVAC)));
}
