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

#include <supla/control/hvac_base.h>

// using ::testing::_;
// using ::testing::ElementsAreArray;
// using ::testing::Args;
// using ::testing::ElementsAre;

TEST(HvacTests, BasicChannelSetup) {
  Supla::Control::HvacBase hvac;

  auto ch = hvac.getChannel();
  ASSERT_NE(ch, nullptr);

  EXPECT_EQ(ch->getChannelNumber(), 0);
  EXPECT_EQ(ch->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  // default function is set in onInit based on supported modes
  // or loaded from config
  EXPECT_EQ(ch->getDefaultFunction(), 0);
  EXPECT_TRUE(ch->isWeeklyScheduleAvailable());
  EXPECT_NE(
      ch->getFlags() & SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE, 0);

  EXPECT_TRUE(hvac.isOnOffSupported());
  EXPECT_TRUE(hvac.isHeatingSupported());
  EXPECT_FALSE(hvac.isCoolingSupported());
  EXPECT_FALSE(hvac.isAutoSupported());
  EXPECT_FALSE(hvac.isFanSupported());
  EXPECT_FALSE(hvac.isDrySupported());

  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);

  // check setters for supported modes
  hvac.setCoolingSupported(true);
  EXPECT_TRUE(hvac.isCoolingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
  // check fan
  hvac.setFanSupported(true);
  EXPECT_TRUE(hvac.isFanSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN);
  // check dry
  hvac.setDrySupported(true);
  EXPECT_TRUE(hvac.isDrySupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY);
  // check auto
  hvac.setAutoSupported(true);
  EXPECT_TRUE(hvac.isAutoSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check onoff
  hvac.setOnOffSupported(false);
  EXPECT_FALSE(hvac.isOnOffSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check heating
  hvac.setHeatingSupported(false);
  EXPECT_FALSE(hvac.isHeatingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check cooling
  hvac.setCoolingSupported(false);
  EXPECT_FALSE(hvac.isCoolingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check fan
  hvac.setFanSupported(false);
  EXPECT_FALSE(hvac.isFanSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check dry
  hvac.setDrySupported(false);
  EXPECT_FALSE(hvac.isDrySupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check auto
  hvac.setAutoSupported(false);
  EXPECT_FALSE(hvac.isAutoSupported());
  EXPECT_EQ(ch->getFuncList(), 0);

  // check onoff
  hvac.setOnOffSupported(true);
  EXPECT_TRUE(hvac.isOnOffSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);

  // check heating
  hvac.setHeatingSupported(true);
  EXPECT_TRUE(hvac.isHeatingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);


  // check if set auto will also set cool and heat
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  EXPECT_FALSE(hvac.isCoolingSupported());
  EXPECT_FALSE(hvac.isHeatingSupported());
  EXPECT_FALSE(hvac.isAutoSupported());

  hvac.setAutoSupported(true);
  EXPECT_TRUE(hvac.isAutoSupported());
  EXPECT_TRUE(hvac.isCoolingSupported());
  EXPECT_TRUE(hvac.isHeatingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);
}

TEST(HvacTests, checkDefaultFunctionInitizedByOnInit) {
  Supla::Control::HvacBase hvac;
  auto *ch = hvac.getChannel();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

  hvac.onInit();
  // check default function
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);

  // check auto
  hvac.setAutoSupported(true);
  // init doesn't change default function when it was previously set
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);


  // clear default function and check auto again
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO);

  // check cool only
  hvac.setCoolingSupported(true);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL);

  // check heat only
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(true);
  hvac.setAutoSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);

  // check dry
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  hvac.setDrySupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_DRYER);

  // check fan
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  hvac.setDrySupported(false);
  hvac.setFanSupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_FAN);

  // check with all options enabled
  hvac.setOnOffSupported(true);
  hvac.setCoolingSupported(true);
  hvac.setHeatingSupported(true);
  hvac.setAutoSupported(true);
  hvac.setDrySupported(true);
  hvac.setFanSupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO);

  // check will all options disabled
  hvac.setOnOffSupported(false);
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  hvac.setDrySupported(false);
  hvac.setFanSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

  // only onoff
  hvac.setOnOffSupported(true);
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  hvac.setDrySupported(false);
  hvac.setFanSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), 0);
}
