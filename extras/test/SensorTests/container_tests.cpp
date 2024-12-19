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

TEST(ContainerTests, ContainerChannelMethods) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::Container container;

  auto channel = container.getChannel();
  EXPECT_EQ(channel->getChannelType(), SUPLA_CHANNELTYPE_CONTAINER);
  EXPECT_EQ(channel->getDefaultFunction(), SUPLA_CHANNELFNC_CONTAINER);

  EXPECT_EQ(channel->getContainerFillValue(), 0);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerValue(102);  // invalid
  EXPECT_EQ(channel->getContainerFillValue(), 0);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerValue(10);
  EXPECT_EQ(channel->getContainerFillValue(), 10);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerValue(0);
  EXPECT_EQ(channel->getContainerFillValue(), 0);
  EXPECT_FALSE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerValue(1);
  channel->setContainerAlarm(true);
  EXPECT_EQ(channel->getContainerFillValue(), 1);
  EXPECT_TRUE(channel->isContainerAlarmActive());
  EXPECT_FALSE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerValue(2);
  channel->setContainerWarning(true);
  EXPECT_EQ(channel->getContainerFillValue(), 2);
  EXPECT_TRUE(channel->isContainerAlarmActive());
  EXPECT_TRUE(channel->isContainerWarningActive());
  EXPECT_FALSE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerValue(3);
  channel->setContainerInvalidSensorState(true);
  EXPECT_EQ(channel->getContainerFillValue(), 3);
  EXPECT_TRUE(channel->isContainerAlarmActive());
  EXPECT_TRUE(channel->isContainerWarningActive());
  EXPECT_TRUE(channel->isContainerInvalidSensorStateActive());
  EXPECT_FALSE(channel->isContainerSoundAlarmOn());

  channel->setContainerValue(4);
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


TEST(ContainerTests, ContainerSetttersAndGetters) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::Container container;

  auto ch = container.getChannel();
  EXPECT_FALSE(ch->isUpdateReady());

  EXPECT_EQ(container.getValue(), 0);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());

  container.setValue(50);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(ch->isUpdateReady());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());

  ch->clearUpdateReady();
  container.setValue(150);  // invalid
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_FALSE(ch->isUpdateReady());

  container.setValue(-1);  // invalid
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  time.advance(2000);
  container.iterateAlways();
  EXPECT_FALSE(ch->isUpdateReady());

  container.setAlarmActive(true);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  container.setWarningActive(true);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  container.setInvalidSensorStateActive(true);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_TRUE(container.isInvalidSensorStateActive());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  container.setSoundAlarmOn(true);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_TRUE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  container.setAlarmActive(false);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_TRUE(container.isWarningActive());
  EXPECT_TRUE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  container.setWarningActive(false);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_TRUE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  container.setInvalidSensorStateActive(false);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_FALSE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());

  container.setAlarmActive(true);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_TRUE(container.isSoundAlarmOn());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  container.setSoundAlarmOn(false);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_TRUE(ch->isUpdateReady());
  ch->clearUpdateReady();

  // setters with no actual change
  container.setAlarmActive(true);
  container.setValue(50);
  container.setWarningActive(false);
  container.setInvalidSensorStateActive(false);
  container.setSoundAlarmOn(false);
  EXPECT_EQ(container.getValue(), 50);
  EXPECT_TRUE(container.isAlarmActive());
  EXPECT_FALSE(container.isWarningActive());
  EXPECT_FALSE(container.isInvalidSensorStateActive());
  EXPECT_FALSE(container.isSoundAlarmOn());

  time.advance(2000);
  container.iterateAlways();
  EXPECT_FALSE(ch->isUpdateReady());
  ch->clearUpdateReady();
}

