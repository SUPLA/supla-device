/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <gtest/gtest.h>

#include <supla/control/thermostat.h>

// using ::testing::_;
// using ::testing::ElementsAreArray;
// using ::testing::Args;
// using ::testing::ElementsAre;

TEST(ThermostatTests, BasicChannelSetup) {
  Supla::Control::Thermostat th;

  auto ch = th.getChannel();
  ASSERT_NE(ch, nullptr);

  EXPECT_EQ(ch->getChannelNumber(), 0);
  EXPECT_EQ(ch->getChannelType(), SUPLA_CHANNELTYPE_THERMOSTAT);
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_THERMOSTAT_HEAT);
  EXPECT_TRUE(ch->isWeeklyScheduleAvailable());
  EXPECT_NE(
      ch->getFlags() & SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE, 0);
}
