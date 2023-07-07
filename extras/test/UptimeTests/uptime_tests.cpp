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
#include <supla/uptime.h>

TEST(UptimeTests, LastResetCauseSetAndGet) {
  Supla::Uptime uptime;
  EXPECT_EQ(uptime.getUptime(), 0);
  EXPECT_EQ(uptime.getConnectionUptime(), 0);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_UNKNOWN);

  // setter should not work unless first resetConnectionUptime is called
  uptime.setConnectionLostCause(SUPLA_LASTCONNECTIONRESETCAUSE_WIFI_CONNECTION_LOST);
  EXPECT_EQ(uptime.getUptime(), 0);
  EXPECT_EQ(uptime.getConnectionUptime(), 0);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_UNKNOWN);

  uptime.resetConnectionUptime();
  uptime.setConnectionLostCause(SUPLA_LASTCONNECTIONRESETCAUSE_WIFI_CONNECTION_LOST);
  EXPECT_EQ(uptime.getUptime(), 0);
  EXPECT_EQ(uptime.getConnectionUptime(), 0);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_WIFI_CONNECTION_LOST);
}

TEST(UptimeTests, IterateShouldIncreaseUptimeCounters) {
  Supla::Uptime uptime;
  uint32_t millis = 0;

  EXPECT_EQ(uptime.getUptime(), 0);
  EXPECT_EQ(uptime.getConnectionUptime(), 0);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_UNKNOWN);

  millis += 999;
  uptime.iterate(millis);
  EXPECT_EQ(uptime.getUptime(), 0);
  EXPECT_EQ(uptime.getConnectionUptime(), 0);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_UNKNOWN);

  millis += 1500;
  uptime.iterate(millis);
  EXPECT_EQ(uptime.getUptime(), 2);
  EXPECT_EQ(uptime.getConnectionUptime(), 2);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_UNKNOWN);

  millis += 20000;
  uptime.iterate(millis);
  EXPECT_EQ(uptime.getUptime(), 22);
  EXPECT_EQ(uptime.getConnectionUptime(), 22);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_UNKNOWN);

  uptime.resetConnectionUptime();
  EXPECT_EQ(uptime.getUptime(), 22);
  EXPECT_EQ(uptime.getConnectionUptime(), 0);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_UNKNOWN);

  millis += 2500;
  uptime.iterate(millis);
  EXPECT_EQ(uptime.getUptime(), 24);
  EXPECT_EQ(uptime.getConnectionUptime(), 2);
  EXPECT_EQ(uptime.getLastResetCause(), SUPLA_LASTCONNECTIONRESETCAUSE_UNKNOWN);
}

TEST(UptimeTests, OverflowTest) {
  Supla::Uptime uptime;
  uint32_t millis = 0;

  EXPECT_EQ(uptime.getUptime(), 0);

  bool wasOverflow = false;
  uint32_t previousMillis = 0;
  for (int i = 0; i < 10000000; i++) {
    millis += 1000;
    uptime.iterate(millis);
    ASSERT_EQ(uptime.getUptime(), i + 1);

    if (previousMillis > millis) {
      wasOverflow = true;
    }
    previousMillis = millis;
  }
  EXPECT_EQ(wasOverflow, true);
}

