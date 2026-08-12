// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla/condition.h>

TEST(OnGreaterEqTests, OnGreaterEqConditionTests) {
  auto cond = OnGreaterEq(20);

  EXPECT_FALSE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(15));

  EXPECT_TRUE(cond->checkConditionFor(20));
  EXPECT_FALSE(cond->checkConditionFor(20.001));
  EXPECT_FALSE(cond->checkConditionFor(25));

  EXPECT_FALSE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(5));
  EXPECT_FALSE(cond->checkConditionFor(5));

  EXPECT_TRUE(cond->checkConditionFor(50));
  EXPECT_FALSE(cond->checkConditionFor(5));

  delete cond;
}

