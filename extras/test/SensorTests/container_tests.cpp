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
#include <supla/sensor/container.h>
#include <simple_time.h>
#include <arduino_mock.h>
#include <supla/events.h>

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};


TEST(ContainerTests, ContainerChannelMethods) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::Container container;

  auto channel = container.getChannel();
  EXPECT_EQ(channel->getChannelType(), SUPLA_CHANNELTYPE_CONTAINER);
  EXPECT_EQ(channel->getDefaultFunction(), SUPLA_CHANNELFNC_CONTAINER);

  EXPECT_EQ(channel->getContainerFillValue(), -1);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerFillValue(102);  // invalid
  EXPECT_EQ(channel->getContainerFillValue(), -1);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerFillValue(10);
  EXPECT_EQ(channel->getContainerFillValue(), 10);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerFillValue(0);
  EXPECT_EQ(channel->getContainerFillValue(), 0);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerFillValue(1);
  channel->setContainerAlarm(true);
  EXPECT_EQ(channel->getContainerFillValue(), 1);
  EXPECT_TRUE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerFillValue(2);
  channel->setContainerWarning(true);
  EXPECT_EQ(channel->getContainerFillValue(), 2);
  EXPECT_TRUE(channel->isContainerAlarmActive());
  EXPECT_TRUE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerFillValue(3);
  channel->setContainerInvalidSensorState(true);
  EXPECT_EQ(channel->getContainerFillValue(), 3);
  EXPECT_TRUE(channel->isContainerAlarmActive());
  EXPECT_TRUE(channel->isContainerWarningActive());
  EXPECT_TRUE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerFillValue(4);
  channel->setContainerSoundAlarmOn(true);
  EXPECT_EQ(channel->getContainerFillValue(), 4);
  EXPECT_TRUE(channel->isContainerAlarmActive());
  EXPECT_TRUE(channel->isContainerWarningActive());
  EXPECT_TRUE(channel->isContainerInvalidSensorStateActive());
  EXPECT_TRUE(channel->isContainerSoundAlarmOn());

  channel->setContainerAlarm(false);
  EXPECT_EQ(channel->getContainerFillValue(), 4);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_TRUE(channel->isContainerWarningActive());
  EXPECT_TRUE(channel->isContainerInvalidSensorStateActive());
  EXPECT_TRUE(channel->isContainerSoundAlarmOn());

  channel->setContainerWarning(false);
  EXPECT_EQ(channel->getContainerFillValue(), 4);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_TRUE(channel->isContainerInvalidSensorStateActive());
  EXPECT_TRUE(channel->isContainerSoundAlarmOn());

  channel->setContainerInvalidSensorState(false);
  EXPECT_EQ(channel->getContainerFillValue(), 4);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_TRUE(channel->isContainerSoundAlarmOn());

  channel->setContainerSoundAlarmOn(false);
  EXPECT_EQ(channel->getContainerFillValue(), 4);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());
}


using ::testing::AtLeast;

TEST(ContainerTests, ContainerSettersAndGetters) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::Container container;

  ActionHandlerMock ah;

  int actionOnChange = 1;
  int actionOnAlarmActive = 2;
  int actionOnAlarmInactive = 3;
  int actionOnWarningActive = 4;
  int actionOnWarningInactive = 5;
  int actionOnInvalidSensorStateActive = 6;
  int actionOnInvalidSensorStateInactive = 7;
  int actionOnSoundAlarmActive = 8;
  int actionOnSoundAlarmInactive = 9;

  container.addAction(actionOnChange, ah, Supla::ON_CHANGE);
  container.addAction(
      actionOnAlarmActive, ah, Supla::ON_CONTAINER_ALARM_ACTIVE);
  container.addAction(
      actionOnAlarmInactive, ah, Supla::ON_CONTAINER_ALARM_INACTIVE);
  container.addAction(
      actionOnWarningActive, ah, Supla::ON_CONTAINER_WARNING_ACTIVE);
  container.addAction(
      actionOnWarningInactive, ah, Supla::ON_CONTAINER_WARNING_INACTIVE);
  container.addAction(actionOnInvalidSensorStateActive,
                      ah,
                      Supla::ON_CONTAINER_INVALID_SENSOR_STATE_ACTIVE);
  container.addAction(actionOnInvalidSensorStateInactive,
                      ah,
                      Supla::ON_CONTAINER_INVALID_SENSOR_STATE_INACTIVE);
  container.addAction(
      actionOnSoundAlarmActive, ah, Supla::ON_CONTAINER_SOUND_ALARM_ACTIVE);
  container.addAction(
      actionOnSoundAlarmInactive, ah, Supla::ON_CONTAINER_SOUND_ALARM_INACTIVE);

  auto ch = container.getChannel();
  EXPECT_FALSE(ch->isUpdateReady());

  EXPECT_EQ(container.readNewValue(), -1);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());

  EXPECT_CALL(ah, handleAction(Supla::ON_CHANGE, actionOnChange)).Times(1);
  container.setValue(50);
  EXPECT_EQ(container.readNewValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(ch->isUpdateReady());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());

  ch->clearUpdateReady();
  EXPECT_CALL(ah, handleAction(Supla::ON_CHANGE, actionOnChange)).Times(1);
  container.setValue(150);  // invalid
  EXPECT_EQ(container.readNewValue(), -1);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  container.setValue(-1);  // invalid
  EXPECT_EQ(container.readNewValue(), -1);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  time.advance(2000);
  container.iterateAlways();
  EXPECT_FALSE(ch->isUpdateReady());
  ch->clearUpdateReady();

  EXPECT_CALL(
      ah, handleAction(Supla::ON_CONTAINER_ALARM_ACTIVE, actionOnAlarmActive))
      .Times(1);
  EXPECT_CALL(ah, handleAction(Supla::ON_CHANGE, actionOnChange)).Times(1);
  container.setAlarmAboveLevel(90);
  container.setValue(100);
  EXPECT_EQ(container.readNewValue(), 100);

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  EXPECT_CALL(
      ah,
      handleAction(Supla::ON_CONTAINER_WARNING_ACTIVE, actionOnWarningActive))
      .Times(1);
  container.setWarningAboveLevel(80);
  EXPECT_EQ(container.readNewValue(), 100);

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  EXPECT_CALL(
      ah, handleAction(Supla::ON_CONTAINER_SOUND_ALARM_ACTIVE,
                       actionOnSoundAlarmActive))
      .Times(1);
  container.setSoundAlarmSupported(true);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_EQ(container.readNewValue(), 100);

  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  EXPECT_CALL(
      ah,
      handleAction(Supla::ON_CONTAINER_ALARM_INACTIVE, actionOnAlarmInactive))
      .Times(1);

  container.setAlarmAboveLevel(0);
  EXPECT_EQ(container.readNewValue(), 100);

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  EXPECT_CALL(ah,
              handleAction(Supla::ON_CONTAINER_WARNING_INACTIVE,
                           actionOnWarningInactive))
      .Times(1);
  EXPECT_CALL(ah,
              handleAction(Supla::ON_CONTAINER_SOUND_ALARM_INACTIVE,
                           actionOnSoundAlarmInactive))
      .Times(1);
  container.setWarningAboveLevel(0);
  EXPECT_EQ(container.readNewValue(), 100);

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());
}

TEST(ContainerTests, AlarmingTests) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::Container container;

  container.setWarningAboveLevel(70);
  container.setAlarmAboveLevel(90);
  container.setWarningBelowLevel(30);
  container.setAlarmBelowLevel(10);

  auto ch = container.getChannel();
  EXPECT_FALSE(ch->isUpdateReady());

  EXPECT_EQ(container.readNewValue(), -1);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());

  container.setValue(50);
  EXPECT_EQ(container.readNewValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(ch->isUpdateReady());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());

  ch->clearUpdateReady();
  container.setValue(70);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_EQ(container.readNewValue(), 70);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setValue(69);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_EQ(container.readNewValue(), 69);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  ch->clearUpdateReady();
  container.setValue(90);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_EQ(container.readNewValue(), 90);
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setValue(79);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_EQ(container.readNewValue(), 79);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setValue(40);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setValue(30);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setValue(0);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setValue(-1);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setValue(0);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setAlarmBelowLevel(0);  // disable
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setWarningBelowLevel(0);  // disable
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setValue(100);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setAlarmAboveLevel(0);  // disable
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  container.setWarningAboveLevel(0);  // disable
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
}

TEST(ContainerTests, SoundAlarmingTests) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::Container container;

  container.setWarningAboveLevel(70);
  container.setAlarmAboveLevel(90);
  container.setWarningBelowLevel(30);
  container.setAlarmBelowLevel(10);
  container.setSoundAlarmSupported(true);

  auto ch = container.getChannel();
  EXPECT_FALSE(ch->isUpdateReady());

  EXPECT_EQ(container.readNewValue(), -1);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());
  container.setValue(50);
  EXPECT_EQ(container.readNewValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(ch->isUpdateReady());

  container.setValue(88);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  container.setValue(97);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  container.setValue(50);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());

  container.setValue(97);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  container.setSoundAlarmSupported(false);
  container.setValue(97);
  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());
}
