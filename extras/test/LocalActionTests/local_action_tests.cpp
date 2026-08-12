// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/local_action.h>
#include <supla/action_handler.h>

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

TEST(LocalActionTests, TwoItemsTests) {
  auto b1 = new Supla::LocalAction;
  auto b2 = new Supla::LocalAction;
  ActionHandlerMock mock1;
  ActionHandlerMock mock2;

  int action1 = 1;
  int action2 = 2;
  int action3 = 3;
  int action4 = 4;
  int action5 = 5;
  int event1 = 11;
  int event2 = 12;
  int event3 = 13;

  EXPECT_CALL(mock1, handleAction(event1, action1));
  EXPECT_CALL(mock1, handleAction(event1, action3));
  EXPECT_CALL(mock2, handleAction(event3, action1));
  EXPECT_CALL(mock1, handleAction(event2, action2));
  EXPECT_CALL(mock1, handleAction(event2, action4));
  EXPECT_CALL(mock1, handleAction(event2, action5));
  EXPECT_CALL(mock2, handleAction(event1, action1));
  EXPECT_CALL(mock2, handleAction(event2, action2));

  b1->addAction(action1, mock1, event1);
  b1->addAction(action2, mock1, event2);
  b1->addAction(action3, mock1, event1);
  b1->addAction(action4, mock1, event2);
  b1->addAction(action5, mock1, event2);
  b1->addAction(action1, mock2, event3);
  b2->addAction(action1, mock2, event1);
  b2->addAction(action2, mock2, event2);
  b1->runAction(event1);
  b1->runAction(event3);
  b1->runAction(event2);

  b2->runAction(event1);

  EXPECT_TRUE(b1->isEventAlreadyUsed(event1, false));
  EXPECT_TRUE(b2->isEventAlreadyUsed(event2, false));

  EXPECT_FALSE(b1->isEventAlreadyUsed(432, false));
  EXPECT_FALSE(b2->isEventAlreadyUsed(event3, false));

  delete b1;

  b2->runAction(event2);

  delete b2;
}

TEST(LocalActionTests, FourItemsTestsNoCalls) {
  auto b1 = new Supla::LocalAction;
  auto b2 = new Supla::LocalAction;
  auto b3 = new Supla::LocalAction;
  auto b4 = new Supla::LocalAction;
  ActionHandlerMock mock1;
  ActionHandlerMock mock2;
  ActionHandlerMock mock3;
  ActionHandlerMock mock4;

  EXPECT_CALL(mock1, handleAction).Times(0);
  EXPECT_CALL(mock2, handleAction).Times(0);
  EXPECT_CALL(mock3, handleAction).Times(0);
  EXPECT_CALL(mock4, handleAction).Times(0);

  int action1 = 1;
  int action2 = 2;
  int action3 = 3;
  int action4 = 4;
  int event1 = 11;
  int event2 = 12;
  int event3 = 13;
  int event4 = 14;

  b4->addAction(action4, mock4, event4);
  b3->addAction(action3, mock3, event3);
  b1->addAction(action1, mock1, event1);
  b2->addAction(action2, mock2, event2);

  b1->runAction(event2);
  b1->runAction(event3);
  b1->runAction(event4);

  b2->runAction(event1);
  b2->runAction(event3);
  b2->runAction(event4);

  b3->runAction(event1);
  b3->runAction(event2);
  b3->runAction(event4);

  b4->runAction(event1);
  b4->runAction(event2);
  b4->runAction(event3);

  delete b1;
  delete b2;
  delete b3;
  delete b4;
}

TEST(LocalActionTests, FourItemsTestsWithCalls) {
  auto b1 = new Supla::LocalAction;
  auto b2 = new Supla::LocalAction;
  auto b3 = new Supla::LocalAction;
  auto b4 = new Supla::LocalAction;
  ActionHandlerMock mock1;
  ActionHandlerMock mock2;
  ActionHandlerMock mock3;
  ActionHandlerMock mock4;

  int action1 = 1;
  int action2 = 2;
  int action3 = 3;
  int action4 = 4;
  int event1 = 11;
  int event2 = 12;
  int event3 = 13;
  int event4 = 14;

  EXPECT_CALL(mock1, handleAction(event1, action1));
  EXPECT_CALL(mock2, handleAction(event2, action2));
  EXPECT_CALL(mock3, handleAction(event3, action3));
  EXPECT_CALL(mock4, handleAction(event4, action4));

  b4->addAction(action4, mock4, event4);
  b3->addAction(action3, mock3, event3);
  b1->addAction(action1, mock1, event1);
  b2->addAction(action2, mock2, event2);

  b1->runAction(event1);
  b1->runAction(event2);
  b1->runAction(event3);
  b1->runAction(event4);

  b2->runAction(event1);
  b2->runAction(event2);
  b2->runAction(event3);
  b2->runAction(event4);

  b3->runAction(event3);
  b3->runAction(event1);
  b3->runAction(event2);
  b3->runAction(event4);

  b4->runAction(event1);
  b4->runAction(event2);
  b4->runAction(event3);
  b4->runAction(event4);

  delete b1;
  delete b2;
  delete b3;
  delete b4;
}

TEST(LocalActionTests, ConfigModeDisableRestoresOnlyPreviouslyEnabledActions) {
  Supla::LocalAction trigger;
  ActionHandlerMock enabledHandler;
  ActionHandlerMock disabledHandler;
  ActionHandlerMock alwaysEnabledHandler;
  constexpr int event = 11;

  trigger.addAction(1, enabledHandler, event);
  trigger.addAction(2, disabledHandler, event);
  trigger.addAction(3, alwaysEnabledHandler, event, true);

  auto enabledClient = trigger.getHandlerForClient(&enabledHandler, event);
  auto disabledClient = trigger.getHandlerForClient(&disabledHandler, event);
  auto alwaysEnabledClient =
      trigger.getHandlerForClient(&alwaysEnabledHandler, event);
  ASSERT_NE(enabledClient, nullptr);
  ASSERT_NE(disabledClient, nullptr);
  ASSERT_NE(alwaysEnabledClient, nullptr);

  disabledClient->disable();
  enabledClient->disableForConfigMode();
  disabledClient->disableForConfigMode();
  alwaysEnabledClient->disableForConfigMode();

  EXPECT_FALSE(enabledClient->isEnabled());
  EXPECT_FALSE(disabledClient->isEnabled());
  EXPECT_TRUE(alwaysEnabledClient->isEnabled());

  enabledClient->restoreAfterConfigMode();
  disabledClient->restoreAfterConfigMode();
  alwaysEnabledClient->restoreAfterConfigMode();

  EXPECT_TRUE(enabledClient->isEnabled());
  EXPECT_FALSE(disabledClient->isEnabled());
  EXPECT_TRUE(alwaysEnabledClient->isEnabled());
}
