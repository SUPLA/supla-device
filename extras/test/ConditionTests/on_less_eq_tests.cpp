// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla/condition.h>

TEST(OnEqualEqTests, OnLessEqConditionTests) {
  auto cond = OnLessEq(10);

  EXPECT_TRUE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(15));

  EXPECT_TRUE(cond->checkConditionFor(10));
  EXPECT_FALSE(cond->checkConditionFor(9.9999));

  // "On" conditions should fire actions only on transition to meet condition.
  EXPECT_FALSE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(5));

  // Going back above threshold value should reset expectation and it should
  // return true on next call with met condition
  EXPECT_FALSE(cond->checkConditionFor(50));
  EXPECT_TRUE(cond->checkConditionFor(5));

  delete cond;
}
