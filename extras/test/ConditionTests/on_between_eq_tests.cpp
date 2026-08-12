// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla/condition.h>

TEST(OnBetweenEqTests, OnBetweenEqConditionTests) {
  auto cond = OnBetweenEq(20, 30);

  EXPECT_FALSE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(15));

  EXPECT_TRUE(cond->checkConditionFor(20));
  EXPECT_FALSE(cond->checkConditionFor(20.001));
  EXPECT_FALSE(cond->checkConditionFor(25));

  EXPECT_FALSE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(5));

  EXPECT_FALSE(cond->checkConditionFor(50));
  EXPECT_TRUE(cond->checkConditionFor(30));
  EXPECT_FALSE(cond->checkConditionFor(5));

  EXPECT_TRUE(cond->checkConditionFor(24));

  delete cond;
}



