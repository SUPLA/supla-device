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

#include <arduino_mock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/channel.h>
#include <supla/clock/clock.h>
#include <supla/element.h>
#include <supla/protocol/supla_srpc.h>
#include <supla_srpc_layer_mock.h>

using ::testing::ElementsAreArray;
using ::testing::Return;

class SuplaSrpcStub : public Supla::Protocol::SuplaSrpc {
 public:
  explicit SuplaSrpcStub(SuplaDeviceClass *sdc)
      : Supla::Protocol::SuplaSrpc(sdc) {
  }

  void setRegisteredAndReady() {
    registered = 1;
  }
};

class ElementTests : public ::testing::Test {
 protected:
  SuplaDeviceClass sd;
  SuplaSrpcStub *suplaSrpc = nullptr;

  virtual void SetUp() {
    if (SuplaDevice.getClock()) {
      delete SuplaDevice.getClock();
    }
    suplaSrpc = new SuplaSrpcStub(&sd);
    suplaSrpc->setRegisteredAndReady();
    Supla::Channel::resetToDefaults();
  }
  virtual void TearDown() {
    delete suplaSrpc;
    Supla::Channel::resetToDefaults();
  }
};

class ElementWithChannel : public Supla::Element {
 public:
  Supla::Channel *getChannel() {
    return &channel;
  }
  const Supla::Channel *getChannel() const {
    return &channel;
  }
  Supla::Channel channel;
};

TEST_F(ElementTests, ElementEmptyListTests) {
  EXPECT_EQ(Supla::Element::begin(), nullptr);
  EXPECT_EQ(Supla::Element::last(), nullptr);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), nullptr);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(-1), nullptr);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(10), nullptr);
}

TEST_F(ElementTests, ElementListAdding) {
  auto el1 = new Supla::Element;

  EXPECT_EQ(Supla::Element::begin(), el1);
  EXPECT_EQ(Supla::Element::last(), el1);

  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), nullptr);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(-1), nullptr);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(10), nullptr);

  auto el2 = new Supla::Element;
  EXPECT_EQ(Supla::Element::begin(), el1);
  EXPECT_EQ(Supla::Element::last(), el2);

  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), nullptr);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(-1), nullptr);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(10), nullptr);

  auto el3 = new Supla::Element;
  EXPECT_EQ(Supla::Element::begin(), el1);
  EXPECT_EQ(Supla::Element::last(), el3);

  delete el2;

  EXPECT_EQ(Supla::Element::begin(), el1);
  EXPECT_EQ(Supla::Element::last(), el3);

  el2 = new Supla::Element;
  EXPECT_EQ(Supla::Element::begin(), el1);
  EXPECT_EQ(Supla::Element::last(), el2);

  delete el1;
  EXPECT_EQ(Supla::Element::begin(), el3);
  EXPECT_EQ(Supla::Element::last(), el2);

  el1 = new Supla::Element;
  EXPECT_EQ(Supla::Element::begin(), el3);
  EXPECT_EQ(Supla::Element::last(), el1);

  delete el1;
  EXPECT_EQ(Supla::Element::begin(), el3);
  EXPECT_EQ(Supla::Element::last(), el2);

  delete el2;
  delete el3;

  EXPECT_EQ(Supla::Element::begin(), nullptr);
  EXPECT_EQ(Supla::Element::last(), nullptr);
}

TEST_F(ElementTests, NoChannelElementMethods) {
  TimeInterfaceMock time;
  Supla::Element el1;

//  EXPECT_CALL(time, millis()).Times(1);

  // those methods are empty, so just call to make sure that they do nothing and
  // don't crash
  el1.onInit();
  el1.onLoadState();
  el1.onSaveState();
  el1.iterateAlways();
  el1.onTimer();
  el1.onFastTimer();

  TDSC_ChannelState channelState;
  el1.handleGetChannelState(&channelState);

  EXPECT_EQ(el1.getChannelNumber(), -1);
  EXPECT_EQ(el1.getChannel(), nullptr);
  EXPECT_EQ(el1.getSecondaryChannel(), nullptr);
  EXPECT_EQ(&(el1.disableChannelState()), &el1);
  EXPECT_EQ(el1.next(), nullptr);

  EXPECT_EQ(el1.iterateConnected(), true);
  EXPECT_EQ(el1.handleNewValueFromServer(nullptr), -1);
  EXPECT_EQ(el1.handleCalcfgFromServer(nullptr),
            SUPLA_CALCFG_RESULT_NOT_SUPPORTED);
}

TEST_F(ElementTests, ChannelElementMethods) {
  ElementWithChannel el1;
  TimeInterfaceMock time;
  SrpcMock srpc;

//  EXPECT_CALL(time, millis()).Times(1);

  // those methods are empty, so just call to make sure that they do nothing and
  // don't crash
  el1.onInit();
  el1.onLoadState();
  el1.onSaveState();
  el1.onTimer();
  el1.onFastTimer();
  el1.onRegistered();

  TDSC_ChannelState channelState = {};
  el1.handleGetChannelState(&channelState);

  EXPECT_EQ(el1.getChannelNumber(), 0);
  EXPECT_EQ(el1.getChannel(), &(el1.channel));
  EXPECT_EQ(el1.getSecondaryChannel(), nullptr);
  EXPECT_EQ(&(el1.disableChannelState()), &el1);
  EXPECT_EQ(el1.next(), nullptr);

  EXPECT_FALSE(el1.channel.isUpdateReady());
  EXPECT_EQ(el1.iterateConnected(), true);
  EXPECT_EQ(el1.handleNewValueFromServer(nullptr), -1);
  EXPECT_EQ(el1.handleCalcfgFromServer(nullptr),
            SUPLA_CALCFG_RESULT_NOT_SUPPORTED);

  EXPECT_FALSE(el1.channel.isUpdateReady());
  el1.channel.setNewValue(true);
  EXPECT_TRUE(el1.channel.isUpdateReady());

//  EXPECT_CALL(time, millis)
//      .WillOnce(Return(0))
//      .WillOnce(Return(200))
//      .WillOnce(Return(250))
//      .WillOnce(Return(250))
//      .WillOnce(Return(400))
//      .WillOnce(Return(600))
//      .WillOnce(Return(800));

  char array0[SUPLA_CHANNELVALUE_SIZE] = {};
  char array1[SUPLA_CHANNELVALUE_SIZE] = {};
  array1[0] = 1;
  EXPECT_CALL(
      srpc,
      valueChanged(nullptr, 0, ElementsAreArray(array1), 0, 0));  // value at #2
  EXPECT_CALL(
      srpc,
      valueChanged(nullptr, 0, ElementsAreArray(array0), 0, 0));  // value at #5
  EXPECT_CALL(srpc, getChannelConfig(0, SUPLA_CONFIG_TYPE_DEFAULT));

  EXPECT_EQ(el1.iterateConnected(), false);  // #1
  EXPECT_EQ(el1.iterateConnected(), true);   // #2

  el1.channel.setNewValue(false);
  EXPECT_EQ(el1.iterateConnected(), false);  // #3
  EXPECT_EQ(el1.iterateConnected(), true);   // #4
  EXPECT_EQ(el1.iterateConnected(), true);   // #5

  EXPECT_FALSE(el1.channel.isUpdateReady());
  el1.channel.setSendGetConfig();
  EXPECT_TRUE(el1.channel.isUpdateReady());
  EXPECT_EQ(el1.iterateConnected(), false);  // #6
  EXPECT_EQ(el1.iterateConnected(), true);   // #7
}

TEST_F(ElementTests, ChannelElementWithWeeklySchedule) {
  ElementWithChannel el1;
  TimeInterfaceMock time;
  SrpcMock srpc;

  EXPECT_CALL(srpc, getChannelConfig(0, SUPLA_CONFIG_TYPE_DEFAULT)).Times(2);

//  EXPECT_CALL(time, millis)
//      .WillOnce(Return(0))    // #1
//      .WillOnce(Return(200))  // #2
//      .WillOnce(Return(250))  // #3
//      .WillOnce(Return(400))  // #4
//      .WillOnce(Return(600))  // #5
//      .WillOnce(Return(800));  // #6

  EXPECT_EQ(el1.iterateConnected(), true);  // #1

  EXPECT_FALSE(el1.channel.isUpdateReady());
  el1.channel.setSendGetConfig();
  EXPECT_TRUE(el1.channel.isUpdateReady());
  EXPECT_EQ(el1.iterateConnected(), false);  // #2
  EXPECT_EQ(el1.iterateConnected(), true);   // #3

  el1.getChannel()->setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  EXPECT_EQ(el1.iterateConnected(), true);  // #4

  EXPECT_FALSE(el1.channel.isUpdateReady());
  el1.channel.setSendGetConfig();
  EXPECT_TRUE(el1.channel.isUpdateReady());
  EXPECT_EQ(el1.iterateConnected(), false);  // #2
  EXPECT_EQ(el1.iterateConnected(), true);   // #3
}

TEST(ElementCaptionTests, InitialCaptionTest) {
  Supla::Channel::resetToDefaults();
  ElementWithChannel el1;
  SimpleTime time;
  SuplaSrpcLayerMock srpc;

  EXPECT_CALL(srpc, isRegisteredAndReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(srpc, setInitialCaption(0, ::testing::StrEq("Test Captiona")));

  el1.setInitialCaption("Test Captiona");

  el1.onRegistered(&srpc);
  el1.iterateConnected();
  Supla::Channel::resetToDefaults();
}
