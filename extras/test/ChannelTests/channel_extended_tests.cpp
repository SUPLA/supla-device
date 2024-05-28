/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <gtest/gtest.h>

#include <supla/channel.h>
#include <supla/channel_extended.h>
#include <gmock/gmock.h>
#include <srpc_mock.h>
#include <supla/events.h>
#include <supla/actions.h>
#include <supla/device/register_device.h>

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};


using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Args;
using ::testing::ElementsAre;

class ChannelExtendedFixture : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Supla::Channel::resetToDefaults();
  }

  virtual void TearDown() {
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(ChannelExtendedFixture, ExtendedChannelMethods) {
  Supla::ChannelExtended extChannel;

  EXPECT_TRUE(extChannel.isExtended());
  EXPECT_NE(nullptr, extChannel.getExtValue() );
}

TEST_F(ChannelExtendedFixture, SetNewValueOnExtChannel) {
  int number = 0;
  Supla::ChannelExtended extChannel;
  TElectricityMeter_ExtendedValue_V2 emVal = {};
  TElectricityMeter_Value expectedValue = {};

  emVal.m_count = 1;
  emVal.measured_values |= EM_VAR_FORWARD_ACTIVE_ENERGY;
  emVal.total_forward_active_energy[0] = 1000;
  emVal.total_forward_active_energy[1] = 2000;
  emVal.total_forward_active_energy[2] = 4000;

  expectedValue.total_forward_active_energy = (1000 + 2000 + 4000) / 1000;

  extChannel.setNewValue(emVal);
  EXPECT_EQ(0, memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                          &expectedValue,
                          sizeof(expectedValue)));
  EXPECT_TRUE(extChannel.isUpdateReady());
  extChannel.clearUpdateReady();

  emVal.measured_values |= EM_VAR_VOLTAGE;
  emVal.m[0].voltage[0] = 10;
  emVal.m[0].voltage[1] = 0;
  emVal.m[0].voltage[2] = 0;

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE1_ON;

  extChannel.setNewValue(emVal);
  EXPECT_EQ(0, memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                          &expectedValue,
                          sizeof(expectedValue)));
  EXPECT_TRUE(extChannel.isUpdateReady());
  extChannel.clearUpdateReady();

  emVal.m[0].voltage[0] = 0;
  emVal.m[0].voltage[1] = 20;
  emVal.m[0].voltage[2] = 0;

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE2_ON;

  extChannel.setNewValue(emVal);
  EXPECT_EQ(0, memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                          &expectedValue,
                          sizeof(expectedValue)));
  EXPECT_TRUE(extChannel.isUpdateReady());
  extChannel.clearUpdateReady();


  emVal.m[0].voltage[0] = 0;
  emVal.m[0].voltage[1] = 0;
  emVal.m[0].voltage[2] = 300;

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE3_ON;

  extChannel.setNewValue(emVal);
  EXPECT_EQ(0, memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                          &expectedValue,
                          sizeof(expectedValue)));
  EXPECT_TRUE(extChannel.isUpdateReady());
  extChannel.clearUpdateReady();


  emVal.m[0].voltage[0] = 10;
  emVal.m[0].voltage[1] = 0;
  emVal.m[0].voltage[2] = 540;

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE1_ON | EM_VALUE_FLAG_PHASE3_ON;

  extChannel.setNewValue(emVal);
  EXPECT_EQ(0, memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                          &expectedValue,
                          sizeof(expectedValue)));
  EXPECT_TRUE(extChannel.isUpdateReady());
  extChannel.clearUpdateReady();

  emVal.m[0].voltage[0] = 10;
  emVal.m[0].voltage[1] = 230;
  emVal.m[0].voltage[2] = 540;

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE1_ON | EM_VALUE_FLAG_PHASE3_ON |
                         EM_VALUE_FLAG_PHASE2_ON;

  extChannel.setNewValue(emVal);
  EXPECT_EQ(0, memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                          &expectedValue,
                          sizeof(expectedValue)));
  EXPECT_TRUE(extChannel.isUpdateReady());
  extChannel.clearUpdateReady();
}
