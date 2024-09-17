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
#include <arduino_mock.h>
#include <simple_time.h>
#include <supla/channel.h>
#include <supla/control/relay.h>
#include <protocol_layer_mock.h>
#include <supla/device/register_device.h>
#include <supla/control/hvac_base.h>
#include <supla/control/internal_pin_output.h>
#include <supla/control/relay_hvac_aggregator.h>

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::Pointee;
using ::testing::AtLeast;

class RelayHvacFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  SimpleTime time;
  ProtocolLayerMock protoMock;

  RelayHvacFixture() {
  }

  virtual ~RelayHvacFixture() {
  }

  void SetUp() {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() {
    Supla::Channel::resetToDefaults();
  }

};

TEST_F(RelayHvacFixture, heatingTest) {
  int gpio1 = 1;
  int gpio2 = 2;
  int gpio3 = 3;
  Supla::Control::Relay r1(gpio1);
  Supla::Control::Relay r2(gpio2);
  Supla::Control::Relay r3(gpio3);

  int number1 = r1.getChannelNumber();
  int number2 = r2.getChannelNumber();
  int number3 = r3.getChannelNumber();
  ASSERT_EQ(number1, 0);
  ASSERT_EQ(number2, 1);
  ASSERT_EQ(number3, 2);

  auto io1 = Supla::Control::InternalPinOutput(4);
  auto io2 = Supla::Control::InternalPinOutput(5);
  auto io3 = Supla::Control::InternalPinOutput(6);
  Supla::Control::HvacBase hvac1(&io1);
  Supla::Control::HvacBase hvac2(&io2);
  Supla::Control::HvacBase hvac3(&io3);

  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number1));

  auto aggregator = Supla::Control::RelayHvacAggregator::Add(number1, &r1);
  EXPECT_NE(aggregator, nullptr);
  EXPECT_EQ(aggregator,
            Supla::Control::RelayHvacAggregator::GetInstance(number1));

  aggregator->registerHvac(&hvac1);
  aggregator->registerHvac(&hvac2);

  // no time advance, nothing happens
  aggregator->iterateAlways();

  // hvacs are off, initial step of relay -> off
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  hvac1.getChannel()->setHvacFlagHeating(true);
  // hvac1 is on and relay is off, so relay -> turn on
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  aggregator->iterateAlways();

  // hvacs are off, but relay is on, so relay -> turn off
  hvac1.getChannel()->setHvacFlagHeating(false);
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  hvac1.getChannel()->setHvacFlagHeating(true);
  hvac2.getChannel()->setHvacFlagHeating(true);
  // hvac1/2 is on and relay is off, so relay -> turn on
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  aggregator->iterateAlways();

  hvac1.getChannel()->setHvacFlagHeating(false);
  // hvac1 is off, hvac2 is on and relay is on
  time.advance(2000);
  aggregator->iterateAlways();

  hvac2.getChannel()->setHvacFlagHeating(false);
  hvac3.getChannel()->setHvacFlagHeating(true);
  // hvac1/2 is off and relay is off, so relay -> off
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  aggregator->registerHvac(&hvac3);
  // hvac3 is on and relay is off, so relay turn on
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  aggregator->iterateAlways();

  aggregator->unregisterHvac(&hvac3);
  // hvac1/2 is off and relay is on, so relay turn off
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  hvac1.getChannel()->setHvacFlagHeating(true);
  aggregator->unregisterHvac(&hvac1);
  time.advance(2000);
  aggregator->iterateAlways();

  // turn on
  hvac2.getChannel()->setHvacFlagHeating(true);
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  aggregator->iterateAlways();

  // turn off on empty list
  aggregator->unregisterHvac(&hvac2);
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  EXPECT_TRUE(Supla::Control::RelayHvacAggregator::Remove(number1));
  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number1));
  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number2));
}

TEST_F(RelayHvacFixture, mixedTest) {
  int gpio1 = 1;
  int gpio2 = 2;
  int gpio3 = 3;
  Supla::Control::Relay r1(gpio1);
  Supla::Control::Relay r2(gpio2);
  Supla::Control::Relay r3(gpio3);

  int number1 = r1.getChannelNumber();
  int number2 = r2.getChannelNumber();
  int number3 = r3.getChannelNumber();
  ASSERT_EQ(number1, 0);
  ASSERT_EQ(number2, 1);
  ASSERT_EQ(number3, 2);

  auto io1 = Supla::Control::InternalPinOutput(4);
  auto io2 = Supla::Control::InternalPinOutput(5);
  auto io3 = Supla::Control::InternalPinOutput(6);
  Supla::Control::HvacBase hvac1(&io1);
  Supla::Control::HvacBase hvac2(&io2);
  Supla::Control::HvacBase hvac3(&io3);

  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number1));

  auto aggregator = Supla::Control::RelayHvacAggregator::Add(number1, &r1);
  EXPECT_NE(aggregator, nullptr);
  EXPECT_EQ(aggregator,
            Supla::Control::RelayHvacAggregator::GetInstance(number1));

  aggregator->registerHvac(&hvac1);
  aggregator->registerHvac(&hvac2);

  // no time advance, nothing happens
  aggregator->iterateAlways();

  // hvacs are off and relay initial off
  time.advance(2000);
//  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  // hvac is on, relay turn on
  hvac1.getChannel()->setHvacFlagCooling(true);
  // hvac1 is on and relay is off, so relay -> turn on
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  aggregator->iterateAlways();

  // hvacs are off, but relay is on, so relay -> turn off
  hvac1.getChannel()->setHvacFlagCooling(false);
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();


  hvac2.getChannel()->setHvacFlagHeating(true);
  // hvac2 is on and relay is off, so relay -> turn on
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  aggregator->iterateAlways();

  hvac1.getChannel()->setHvacFlagCooling(false);
  // hvac2 is on, so relay remains on
  time.advance(2000);
  aggregator->iterateAlways();

  hvac2.getChannel()->setHvacFlagHeating(false);
  hvac3.getChannel()->setHvacFlagCooling(true);
  // hvac1/2 is off, so relay -> turn off
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  aggregator->registerHvac(&hvac3);
  // hvac3 is on and relay is off, so relay turn on
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  aggregator->iterateAlways();

  aggregator->unregisterHvac(&hvac3);
  // hvac1/2 is off and relay is on, so relay turn off
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  hvac1.getChannel()->setHvacFlagHeating(true);
  aggregator->unregisterHvac(&hvac1);
  time.advance(2000);
  aggregator->iterateAlways();

  hvac2.getChannel()->setHvacFlagHeating(true);
  aggregator->unregisterHvac(&hvac2);
  time.advance(2000);
  aggregator->iterateAlways();

  EXPECT_TRUE(Supla::Control::RelayHvacAggregator::Remove(number1));
  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number1));
  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number2));
}

TEST_F(RelayHvacFixture, turnOffWhenEmptyTest) {
  int gpio1 = 1;
  int gpio2 = 2;
  int gpio3 = 3;
  Supla::Control::Relay r1(gpio1);
  Supla::Control::Relay r2(gpio2);
  Supla::Control::Relay r3(gpio3);

  int number1 = r1.getChannelNumber();
  int number2 = r2.getChannelNumber();
  int number3 = r3.getChannelNumber();
  ASSERT_EQ(number1, 0);
  ASSERT_EQ(number2, 1);
  ASSERT_EQ(number3, 2);

  auto io1 = Supla::Control::InternalPinOutput(4);
  auto io2 = Supla::Control::InternalPinOutput(5);
  auto io3 = Supla::Control::InternalPinOutput(6);
  Supla::Control::HvacBase hvac1(&io1);
  Supla::Control::HvacBase hvac2(&io2);
  Supla::Control::HvacBase hvac3(&io3);

  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number1));

  auto aggregator = Supla::Control::RelayHvacAggregator::Add(number1, &r1);
  EXPECT_NE(aggregator, nullptr);
  EXPECT_EQ(aggregator,
            Supla::Control::RelayHvacAggregator::GetInstance(number1));

  aggregator->registerHvac(&hvac1);
  aggregator->registerHvac(&hvac2);

  // no time advance, nothing happens
  aggregator->iterateAlways();

  // hvacs are off , intial turn off
  time.advance(2000);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  hvac1.getChannel()->setHvacFlagHeating(true);
  // hvac1 is on and relay is off, so relay -> turn on
  time.advance(2000);
//  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  aggregator->iterateAlways();

  aggregator->unregisterHvac(&hvac1);
  aggregator->unregisterHvac(&hvac2);
  // no hvac registered, but output is on -> turn off
  time.advance(2000);
//  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  aggregator->iterateAlways();

  // no hvac registered, output is off -> nothing
  time.advance(2000);
//  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(0));
  aggregator->iterateAlways();

  // change aggregator behavior -> turnOffWhenEmpty(false)
  // nothing should happen, regardless of relay output state
  aggregator->setTurnOffWhenEmpty(false);
  time.advance(2000);
//  EXPECT_CALL(ioMock, digitalRead(gpio1)).Times(0);
  aggregator->iterateAlways();

  time.advance(2000);
//  EXPECT_CALL(ioMock, digitalRead(gpio1)).Times(0);
  aggregator->iterateAlways();

  EXPECT_TRUE(Supla::Control::RelayHvacAggregator::Remove(number1));
  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number1));
  EXPECT_FALSE(Supla::Control::RelayHvacAggregator::Remove(number2));
}
