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

#include <supla/correction.h>

using namespace Supla;

TEST(CorrectionTests, CorrectionGetCheck) {
  Correction::add(5, 2.5);

  EXPECT_EQ(Correction::get(5), 2.5);
  EXPECT_EQ(Correction::get(5, true), 0);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(1, 3.14);
  EXPECT_EQ(Correction::get(1), 3.14);
  EXPECT_EQ(Correction::get(5), 2.5);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}

TEST(CorrectionTests, CorrectionGetCheckForSecondary) {
  Correction::add(5, 2.5, true);

  EXPECT_EQ(Correction::get(5), 0);
  EXPECT_EQ(Correction::get(5, true), 2.5);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(1, 3.14);
  EXPECT_EQ(Correction::get(1), 3.14);
  EXPECT_EQ(Correction::get(5, true), 2.5);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}

TEST(CorrectionTests, CorrectionChangeTest) {
  Correction::add(5, 2.5);

  EXPECT_EQ(Correction::get(5), 2.5);
  EXPECT_EQ(Correction::get(5, true), 0);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(5, 3.14);
  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 3.14);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}
