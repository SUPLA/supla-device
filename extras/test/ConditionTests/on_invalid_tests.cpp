// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <supla/condition.h>
#include <supla/channel_element.h>
#include <supla/events.h>
#include <supla/actions.h>
#include <supla/device/register_device.h>


class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};


using ::testing::_;

TEST(OnInvalidTests, OnInvalidCondition) {
  Supla::RegisterDevice::resetToDefaults();
  ActionHandlerMock ahMock;
  const int action1 = 15;
  const int action2 = 16;
  const int action3 = 17;
  const int action4 = 18;
  const int action5 = 19;
  const int action6 = 20;

  EXPECT_CALL(ahMock, handleAction(Supla::ON_CHANGE, action3)).Times(1);
  EXPECT_CALL(ahMock, handleAction(Supla::ON_CHANGE, action6)).Times(1);

  Supla::ChannelElement channelElement;
  auto channel = channelElement.getChannel();

  auto cond = OnInvalid();
  cond->setSource(channelElement);
  cond->setClient(ahMock);


  channel->setType(SUPLA_CHANNELTYPE_THERMOMETER);

  channel->setNewValue(0.0);
  // channel should be initialized to 0, so condition is not met
  cond->handleAction(Supla::ON_CHANGE, action1);

  // 100 is valid , nothing should happen
  channel->setNewValue(100.0);
  cond->handleAction(Supla::ON_CHANGE, action2);

  // invalid valid, should trigger action
  channel->setNewValue(-275.0);
  cond->handleAction(Supla::ON_CHANGE, action3);

  // it is still invalid, so nothing should happen
  cond->handleAction(Supla::ON_CHANGE, action4);

  // nothing should happen
  channel->setNewValue(25.0);
  cond->handleAction(Supla::ON_CHANGE, action5);

  // invalid valid, should trigger action
  channel->setNewValue(-275.0);
  cond->handleAction(Supla::ON_CHANGE, action6);

  delete cond;
}
