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

#include <supla/element.h>
#include <supla/channel.h>
#include <arduino_mock.h>
#include <srpc_mock.h>
#include <supla/protocol/supla_srpc.h>
#include <network_client_mock.h>

using ::testing::Return;
using ::testing::ElementsAreArray;

class SuplaSrpcStub : public Supla::Protocol::SuplaSrpc {
 public:
  SuplaSrpcStub(SuplaDeviceClass *sdc) : Supla::Protocol::SuplaSrpc(sdc) {
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
      new NetworkClientMock;  // it will be destroyed in
                              // Supla::Protocol::SuplaSrpc
      suplaSrpc = new SuplaSrpcStub(&sd);
      suplaSrpc->setRegisteredAndReady();
      Supla::Channel::lastCommunicationTimeMs = 0;
      memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    }
    virtual void TearDown() {
      delete suplaSrpc;
      Supla::Channel::lastCommunicationTimeMs = 0;
      memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    }

};


class ElementWithChannel : public Supla::Element {
  public:
    Supla::Channel *getChannel() {
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

  EXPECT_CALL(time, millis()).Times(1);

  // those methods are empty, so just call to make sure that they do nothing and don't crash
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
  EXPECT_EQ(el1.handleCalcfgFromServer(nullptr), SUPLA_CALCFG_RESULT_NOT_SUPPORTED);
}

TEST_F(ElementTests, ChannelElementMethods) {
  ElementWithChannel el1;
  TimeInterfaceMock time;
  SrpcMock srpc;

  EXPECT_CALL(time, millis()).Times(1);

  // those methods are empty, so just call to make sure that they do nothing and don't crash
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
  EXPECT_EQ(el1.handleCalcfgFromServer(nullptr), SUPLA_CALCFG_RESULT_NOT_SUPPORTED);

  EXPECT_FALSE(el1.channel.isUpdateReady());
  el1.channel.setNewValue(true);
  EXPECT_TRUE(el1.channel.isUpdateReady());

  EXPECT_CALL(time, millis)
    .WillOnce(Return(0))   // #1 first call after value changed to true
    .WillOnce(Return(200)) // #2 two calls after value changed to true and 100 ms passed
    .WillOnce(Return(250)) // #3 value changed, however not enough time passed
    .WillOnce(Return(250)) // #4 value changed, however not enough time passed
    .WillOnce(Return(400)) // #5 two calls after value changed and another >100 ms passed
    .WillOnce(Return(600))
    .WillOnce(Return(800));

  char array0[SUPLA_CHANNELVALUE_SIZE] = {};
  char array1[SUPLA_CHANNELVALUE_SIZE] = {};
  array1[0] = 1;
  EXPECT_CALL(srpc, valueChanged(nullptr, 0, ElementsAreArray(array1), 0, 0)); // value at #2
  EXPECT_CALL(srpc, valueChanged(nullptr, 0, ElementsAreArray(array0), 0, 0)); // value at #5
  EXPECT_CALL(srpc, getChannelConfig(0, SUPLA_CONFIG_TYPE_DEFAULT));


  EXPECT_EQ(el1.iterateConnected(), true);  // #1
  EXPECT_EQ(el1.iterateConnected(), false); // #2

  el1.channel.setNewValue(false);
  EXPECT_EQ(el1.iterateConnected(), true);  // #3
  EXPECT_EQ(el1.iterateConnected(), true);  // #4
  EXPECT_EQ(el1.iterateConnected(), false); // #5

  EXPECT_FALSE(el1.channel.isUpdateReady());
  el1.channel.requestChannelConfig();
  EXPECT_TRUE(el1.channel.isUpdateReady());
  EXPECT_EQ(el1.iterateConnected(), false);  // #6
  EXPECT_EQ(el1.iterateConnected(), true);  // #7
}

TEST_F(ElementTests, ChannelElementWithWeeklySchedule) {
  ElementWithChannel el1;
  TimeInterfaceMock time;
  SrpcMock srpc;

  EXPECT_CALL(srpc, getChannelConfig(0, SUPLA_CONFIG_TYPE_DEFAULT)).
    Times(2);

  EXPECT_CALL(time, millis)
      .WillOnce(Return(0))    // #1
      .WillOnce(Return(200))  // #2
      .WillOnce(Return(250))  // #3
      .WillOnce(Return(400))  // #4
      .WillOnce(Return(600))  // #5
      .WillOnce(Return(800))  // #6
      ;

  EXPECT_EQ(el1.iterateConnected(), true);  // #1

  EXPECT_FALSE(el1.channel.isUpdateReady());
  el1.channel.requestChannelConfig();
  EXPECT_TRUE(el1.channel.isUpdateReady());
  EXPECT_EQ(el1.iterateConnected(), false);  // #2
  EXPECT_EQ(el1.iterateConnected(), true);  // #3

  el1.getChannel()->setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  EXPECT_EQ(el1.iterateConnected(), true);  // #4

  EXPECT_FALSE(el1.channel.isUpdateReady());
  el1.channel.requestChannelConfig();
  EXPECT_TRUE(el1.channel.isUpdateReady());
  EXPECT_EQ(el1.iterateConnected(), false);  // #2
  EXPECT_EQ(el1.iterateConnected(), true);  // #3
}




