/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/control/valve_base.h>
#include <supla/control/virtual_valve.h>
#include <simple_time.h>
#include <arduino_mock.h>
#include <supla/events.h>
#include "supla/sensor/virtual_binary.h"

using ::testing::AtLeast;

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};


TEST(ValveTests, ValveOpenCloseChannelMethods) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Control::ValveBase valve;

  auto channel = valve.getChannel();
  EXPECT_EQ(channel->getChannelType(), SUPLA_CHANNELTYPE_VALVE_OPENCLOSE);
  EXPECT_EQ(channel->getDefaultFunction(), SUPLA_CHANNELFNC_VALVE_OPENCLOSE);

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  channel->setValveOpenState(0);
  EXPECT_EQ(channel->getValveOpenState(), 0);
  channel->setValveOpenState(1);
  EXPECT_EQ(channel->getValveOpenState(), 100);
  channel->setValveOpenState(50);
  EXPECT_EQ(channel->getValveOpenState(), 100);
  channel->setValveOpenState(100);
  EXPECT_EQ(channel->getValveOpenState(), 100);
  channel->setValveOpenState(101);
  EXPECT_EQ(channel->getValveOpenState(), 100);

  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  channel->setValveFloodingFlag(true);
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  channel->setValveManuallyClosedFlag(true);
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  channel->setValveFloodingFlag(false);
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());
  channel->setValveManuallyClosedFlag(false);
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  channel->setValveManuallyClosedFlag(true);
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  channel->setValveFloodingFlag(true);
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  channel->setValveOpenState(0);
  EXPECT_EQ(channel->getValveOpenState(), 0);
  channel->setValveOpenState(1);
  EXPECT_EQ(channel->getValveOpenState(), 100);
  channel->setValveOpenState(50);
  EXPECT_EQ(channel->getValveOpenState(), 100);
  channel->setValveOpenState(100);
  EXPECT_EQ(channel->getValveOpenState(), 100);
  channel->setValveOpenState(101);
  EXPECT_EQ(channel->getValveOpenState(), 100);

  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());
}

TEST(ValveTests, ValvePercentageChannelMethods) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Control::ValveBase valve(false);

  auto channel = valve.getChannel();
  EXPECT_EQ(channel->getChannelType(), SUPLA_CHANNELTYPE_VALVE_PERCENTAGE);
  EXPECT_EQ(channel->getDefaultFunction(), SUPLA_CHANNELFNC_VALVE_PERCENTAGE);

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

    channel->setValveOpenState(0);
  EXPECT_EQ(channel->getValveOpenState(), 0);
  channel->setValveOpenState(1);
  EXPECT_EQ(channel->getValveOpenState(), 1);
  channel->setValveOpenState(50);
  EXPECT_EQ(channel->getValveOpenState(), 50);
  channel->setValveOpenState(100);
  EXPECT_EQ(channel->getValveOpenState(), 100);
  channel->setValveOpenState(101);
  EXPECT_EQ(channel->getValveOpenState(), 100);

  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  channel->setValveFloodingFlag(true);
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  channel->setValveManuallyClosedFlag(true);
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  channel->setValveFloodingFlag(false);
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  channel->setValveManuallyClosedFlag(false);
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  channel->setValveManuallyClosedFlag(true);
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  channel->setValveFloodingFlag(true);
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  channel->setValveOpenState(0);
  EXPECT_EQ(channel->getValveOpenState(), 0);
  channel->setValveOpenState(1);
  EXPECT_EQ(channel->getValveOpenState(), 1);
  channel->setValveOpenState(50);
  EXPECT_EQ(channel->getValveOpenState(), 50);
  channel->setValveOpenState(100);
  EXPECT_EQ(channel->getValveOpenState(), 100);
  channel->setValveOpenState(101);
  EXPECT_EQ(channel->getValveOpenState(), 100);

  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());
}

TEST(ValveTests, ValveElementTests) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Control::VirtualValve valve;
  Supla::Sensor::VirtualBinary vb1;
  Supla::Sensor::VirtualBinary vb2;

  auto channel = valve.getChannel();
  EXPECT_EQ(channel->getChannelType(), SUPLA_CHANNELTYPE_VALVE_OPENCLOSE);
  EXPECT_EQ(channel->getDefaultFunction(), SUPLA_CHANNELFNC_VALVE_OPENCLOSE);

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());

  valve.onLoadConfig(nullptr);
  valve.onLoadState();
  valve.onInit();
  vb1.onLoadConfig(nullptr);
  vb1.onLoadState();
  vb1.onInit();
  vb2.onLoadConfig(nullptr);
  vb2.onLoadState();
  vb2.onInit();

  vb1.clear();
  vb2.clear();

  // load state defaults valve open state to "0"
  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());

  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());

  valve.setValueOnDevice(0);
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  valve.setValueOnDevice(10);
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  // check manually closed flag
  valve.setValueOnDevice(0);
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  TSD_SuplaChannelNewValue newValueFromServer = {};
  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = valve.getChannelNumber();
  newValueFromServer.value[0] = 1;  // open

  EXPECT_EQ(1, valve.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  newValueFromServer.value[0] = 0;  // Close

  EXPECT_EQ(1, valve.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  newValueFromServer.value[0] = 1;  // open
  EXPECT_EQ(1, valve.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());


  valve.setValueOnDevice(10);
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  valve.setValueOnDevice(0);
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_TRUE(channel->isValveManuallyClosedFlagActive());

  // move time by 40s to clear ignoring of manually closed flag check after
  // request from server - by default it is set to 30s.
  for (int i = 0; i < 4; i++) {
    time.advance(10000);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  valve.setValueOnDevice(100);
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  // Check flooding flag
  newValueFromServer.value[0] = 1;  // open
  EXPECT_EQ(1, valve.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  valve.addSensor(vb1.getChannelNumber());
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  // Flood detected
  vb1.set();
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  // try to open, when flood detection is active
  newValueFromServer.value[0] = 1;  // open
  EXPECT_EQ(1, valve.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  vb1.clear();
  // no more flood detection, but flag remains
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  // open clears flag
  EXPECT_EQ(1, valve.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  // add vb2
  vb2.set();
  valve.addSensor(vb2.getChannelNumber());
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());
}

TEST(ValveTests, ValveOnChangeTests) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Control::VirtualValve valve;
  valve.setDefaultCloseValveOnFloodType(
      SUPLA_VALVE_CLOSE_ON_FLOOD_TYPE_ON_CHANGE);
  Supla::Sensor::VirtualBinary vb1;
  Supla::Sensor::VirtualBinary vb2;

  auto channel = valve.getChannel();
  EXPECT_EQ(channel->getChannelType(), SUPLA_CHANNELTYPE_VALVE_OPENCLOSE);
  EXPECT_EQ(channel->getDefaultFunction(), SUPLA_CHANNELFNC_VALVE_OPENCLOSE);

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());

  valve.onLoadConfig(nullptr);
  valve.onLoadState();
  valve.onInit();
  vb1.onLoadConfig(nullptr);
  vb1.onLoadState();
  vb1.onInit();
  vb2.onLoadConfig(nullptr);
  vb2.onLoadState();
  vb2.onInit();

  vb1.clear();
  vb2.clear();

  // load state defaults valve open state to "0"
  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());

  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());

  valve.setValueOnDevice(100);
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  valve.addSensor(vb1.getChannelNumber());
  valve.addSensor(vb2.getChannelNumber());
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  // Flood detected
  vb1.set();
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  // try to open, when flood detection is active
  TSD_SuplaChannelNewValue newValueFromServer = {};
  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = valve.getChannelNumber();
  newValueFromServer.value[0] = 1;  // open
  EXPECT_EQ(1, valve.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  // it should be open, becuase we only close on flooding state change
  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  vb2.set();
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  vb1.clear();
  vb2.clear();
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 0);
  EXPECT_FALSE(channel->isValveOpen());
  EXPECT_TRUE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

  EXPECT_EQ(1, valve.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 10; i++) {
    time.advance(200);
    valve.iterateAlways();
    vb1.iterateAlways();
    vb2.iterateAlways();
  }

  EXPECT_EQ(channel->getValveOpenState(), 100);
  EXPECT_TRUE(channel->isValveOpen());
  EXPECT_FALSE(channel->isValveFloodingFlagActive());
  EXPECT_FALSE(channel->isValveManuallyClosedFlagActive());

}
