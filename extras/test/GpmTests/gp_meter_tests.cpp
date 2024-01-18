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

#include <arduino_mock.h>
#include <gtest/gtest.h>
#include <supla/sensor/general_purpose_meter.h>
#include <measurement_driver_mock.h>
#include <config_mock.h>
#include <simple_time.h>
#include "supla/sensor/general_purpose_channel_base.h"

using ::testing::Return;
using ::testing::_;
using ::testing::StrEq;
using ::testing::DoAll;
using ::testing::SetArgPointee;

class GpMeterTestsFixture : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }

  virtual void TearDown() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
};


TEST_F(GpMeterTestsFixture, initNaNValue) {
  MeasurementDriverMock driver;
  Supla::Sensor::GeneralPurposeMeter gp(&driver);

  EXPECT_CALL(driver, initialize()).Times(1);
  EXPECT_CALL(driver, getValue()).Times(1).WillOnce(Return(NAN));

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 0);

  gp.onInit();
  EXPECT_TRUE(std::isnan(gp.getChannel()->getValueDouble()));
}

TEST_F(GpMeterTestsFixture, initValue) {
  MeasurementDriverMock driver;
  Supla::Sensor::GeneralPurposeMeter gp(&driver);

  EXPECT_CALL(driver, initialize()).Times(1);
  EXPECT_CALL(driver, getValue()).Times(1).WillOnce(Return(3.1415));

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 0);

  gp.onInit();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 3.1415);
}

TEST_F(GpMeterTestsFixture, valueUpdateDefaultRefresh) {
  MeasurementDriverMock driver;
  Supla::Sensor::GeneralPurposeMeter gp(&driver);
  SimpleTime time;

  EXPECT_CALL(driver, initialize()).Times(1);
  EXPECT_CALL(driver, getValue()).WillOnce(Return(1)).
    WillOnce(Return(2)).
    WillOnce(Return(3));

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 0);

  gp.onInit();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  time.advance(500);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  time.advance(4501);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(500);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(500);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(1000);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(1000);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(1500);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(2500);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 3);

  EXPECT_EQ(gp.getRefreshIntervalMs(), 0);
}

TEST_F(GpMeterTestsFixture, valueUpdateQuickRefresh) {
  MeasurementDriverMock driver;
  Supla::Sensor::GeneralPurposeMeter gp(&driver);
  SimpleTime time;

  EXPECT_CALL(driver, initialize()).Times(1);
  EXPECT_CALL(driver, getValue()).WillOnce(Return(1)).
    WillOnce(Return(2)).
    WillOnce(Return(3));

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 0);

  gp.setRefreshIntervalMs(500);
  gp.onInit();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  time.advance(501);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(501);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 3);

  EXPECT_EQ(gp.getRefreshIntervalMs(), 500);
}

TEST_F(GpMeterTestsFixture, valueUpdate200Refresh) {
  MeasurementDriverMock driver;
  Supla::Sensor::GeneralPurposeMeter gp(&driver);
  SimpleTime time;

  EXPECT_CALL(driver, initialize()).Times(1);
  EXPECT_CALL(driver, getValue()).WillOnce(Return(1)).
    WillOnce(Return(2)).
    WillOnce(Return(3));

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 0);

  // 100 ms is too fast, channel should set 200 ms
  gp.setRefreshIntervalMs(100);
  gp.onInit();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  time.advance(101);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  time.advance(101);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(201);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 3);

  // in config it should stay 100
  EXPECT_EQ(gp.getRefreshIntervalMs(), 100);
}

TEST_F(GpMeterTestsFixture, valueUpdateRestoreDefaultRefresh) {
  MeasurementDriverMock driver;
  Supla::Sensor::GeneralPurposeMeter gp(&driver);
  SimpleTime time;

  EXPECT_CALL(driver, initialize()).Times(1);
  EXPECT_CALL(driver, getValue()).WillOnce(Return(1)).
    WillOnce(Return(2)).
    WillOnce(Return(3));

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 0);

  // 0 should set defualt 5s
  gp.setRefreshIntervalMs(0);
  gp.onInit();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  time.advance(4999);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 1);

  time.advance(2);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 2);

  time.advance(5001);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 3);

  // in config it should stay 0
  EXPECT_EQ(gp.getRefreshIntervalMs(), 0);
}
TEST_F(GpMeterTestsFixture, initialConfigValues) {
  Supla::Sensor::GeneralPurposeMeter gp;

  EXPECT_EQ(gp.getDefaultValueDivider(), 0);
  EXPECT_EQ(gp.getDefaultValueMultiplier(), 0);
  EXPECT_EQ(gp.getDefaultValueAdded(), 0);
  EXPECT_EQ(gp.getDefaultValuePrecision(), 0);
  EXPECT_EQ(gp.getRefreshIntervalMs(), 0);
  EXPECT_EQ(gp.getValueDivider(), 0);
  EXPECT_EQ(gp.getValueMultiplier(), 0);
  EXPECT_EQ(gp.getValueAdded(), 0);
  EXPECT_EQ(gp.getValuePrecision(), 0);
  EXPECT_EQ(gp.getNoSpaceBeforeValue(), 0);
  EXPECT_EQ(gp.getNoSpaceAfterValue(), 0);
  EXPECT_EQ(gp.getKeepHistory(), 0);
  EXPECT_EQ(gp.getChartType(), 0);
  EXPECT_EQ(gp.getCounterType(), 0);
  EXPECT_EQ(gp.getFillMissingData(), 0);
  EXPECT_EQ(gp.getIncludeValueAddedInHistory(), 0);

  char testString[100] = {};
  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getDefaultUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getDefaultUnitAfterValue(testString);
  EXPECT_STREQ(testString, "");
}

TEST_F(GpMeterTestsFixture, setConfigValues) {
  Supla::Sensor::GeneralPurposeMeter gp;

  // set some value - they may be not proper for a given parameter,
  // but we don't validate them
  gp.setDefaultValueDivider(1);
  gp.setDefaultValueMultiplier(2);
  gp.setDefaultValueAdded(3);
  gp.setDefaultValuePrecision(4);
  gp.setRefreshIntervalMs(5);
  gp.setValueDivider(6);
  gp.setValueMultiplier(7);
  gp.setValueAdded(8);
  gp.setValuePrecision(9);
  gp.setNoSpaceBeforeValue(10);
  gp.setNoSpaceAfterValue(11);
  gp.setKeepHistory(12);
  gp.setChartType(13);
  gp.setCounterType(14);
  gp.setFillMissingData(15);
  gp.setIncludeValueAddedInHistory(16);

  EXPECT_EQ(gp.getDefaultValueDivider(), 1);
  EXPECT_EQ(gp.getDefaultValueMultiplier(), 2);
  EXPECT_EQ(gp.getDefaultValueAdded(), 3);
  EXPECT_EQ(gp.getDefaultValuePrecision(), 4);
  EXPECT_EQ(gp.getRefreshIntervalMs(), 5);
  EXPECT_EQ(gp.getValueDivider(), 6);
  EXPECT_EQ(gp.getValueMultiplier(), 7);
  EXPECT_EQ(gp.getValueAdded(), 8);
  EXPECT_EQ(gp.getValuePrecision(), 9);
  EXPECT_EQ(gp.getNoSpaceBeforeValue(), 10);
  EXPECT_EQ(gp.getNoSpaceAfterValue(), 11);
  EXPECT_EQ(gp.getKeepHistory(), 12);
  EXPECT_EQ(gp.getChartType(), 13);
  EXPECT_EQ(gp.getCounterType(), 14);
  EXPECT_EQ(gp.getFillMissingData(), 15);
  EXPECT_EQ(gp.getIncludeValueAddedInHistory(), 16);

  gp.setDefaultValueDivider(-1);
  gp.setDefaultValueMultiplier(-2);
  gp.setDefaultValueAdded(-3);
  gp.setValueDivider(-5);
  gp.setValueMultiplier(-6);
  gp.setValueAdded(-7);

  EXPECT_EQ(gp.getDefaultValueDivider(), -1);
  EXPECT_EQ(gp.getDefaultValueMultiplier(), -2);
  EXPECT_EQ(gp.getDefaultValueAdded(), -3);
  EXPECT_EQ(gp.getValueDivider(), -5);
  EXPECT_EQ(gp.getValueMultiplier(), -6);
  EXPECT_EQ(gp.getValueAdded(), -7);

  gp.setRefreshIntervalMs(-4);
  EXPECT_EQ(gp.getRefreshIntervalMs(), 0);

  gp.setValueAdded(1000000000000);
  EXPECT_EQ(gp.getValueAdded(), 1000000000000);

  gp.setValueAdded(-1000000000000);
  EXPECT_EQ(gp.getValueAdded(), -1000000000000);

  gp.setDefaultValueAdded(1000000000000);
  EXPECT_EQ(gp.getDefaultValueAdded(), 1000000000000);

  gp.setUnitBeforeValue("unit before VALUE");
  gp.setUnitAfterValue("unit after VALUE");
  gp.setDefaultUnitBeforeValue("default unit before VALUE");
  gp.setDefaultUnitAfterValue("default unit after VALUE");

  char testString[100] = {};
  gp.getUnitBeforeValue(testString);
  // values are truncated to 15 bytes
  EXPECT_STREQ(testString, "unit before VAL");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "unit after VALU");

  gp.getDefaultUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "default unit be");

  gp.getDefaultUnitAfterValue(testString);
  EXPECT_STREQ(testString, "default unit af");

  gp.setUnitBeforeValue("");
  gp.setUnitAfterValue("");
  gp.setDefaultUnitBeforeValue("");
  gp.setDefaultUnitAfterValue("");
  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getDefaultUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getDefaultUnitAfterValue(testString);
  EXPECT_STREQ(testString, "");

  gp.setUnitBeforeValue("1");
  gp.setUnitAfterValue("2");
  gp.setDefaultUnitBeforeValue("3");
  gp.setDefaultUnitAfterValue("4");
  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "1");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "2");

  gp.getDefaultUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "3");

  gp.getDefaultUnitAfterValue(testString);
  EXPECT_STREQ(testString, "4");
}

TEST_F(GpMeterTestsFixture, defaultParametersShouldInitializeConfig) {
  Supla::Sensor::GeneralPurposeMeter gp;

  gp.setDefaultValueAdded(1);
  gp.setDefaultValueDivider(2);
  gp.setDefaultValueMultiplier(3);
  gp.setDefaultValuePrecision(4);
  gp.setDefaultUnitBeforeValue("before");
  gp.setDefaultUnitAfterValue("after");

  EXPECT_EQ(gp.getValueAdded(), 0);
  EXPECT_EQ(gp.getValueDivider(), 0);
  EXPECT_EQ(gp.getValueMultiplier(), 0);
  EXPECT_EQ(gp.getValuePrecision(), 0);

  char testString[100] = {};
  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "");

  gp.onLoadConfig(nullptr);
  gp.onInit();

  EXPECT_EQ(gp.getValueAdded(), 1);
  EXPECT_EQ(gp.getValueDivider(), 2);
  EXPECT_EQ(gp.getValueMultiplier(), 3);
  EXPECT_EQ(gp.getValuePrecision(), 4);

  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "before");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "after");
}

TEST_F(GpMeterTestsFixture, defaultParametersShouldntOverwriteConfig) {
  Supla::Sensor::GeneralPurposeMeter gp;
  ConfigMock config;

  EXPECT_CALL(config, saveWithDelay(_)).Times(0);
  EXPECT_CALL(
      config,
      getBlob(
          StrEq("0_gpm_common"),
          _,
          sizeof(Supla::Sensor::GeneralPurposeChannelBase::GPMCommonConfig)))
      .Times(1)
      .WillOnce([](const char *key, char *value, size_t blobSize) {
        Supla::Sensor::GeneralPurposeChannelBase::GPMCommonConfig config = {};
        EXPECT_EQ(blobSize, sizeof(config));
        config.added = 11;
        config.divider = 12;
        config.multiplier = 13;
        config.precision = 14;
        strncpy(
            config.unitBeforeValue, "test1", SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
        strncpy(
            config.unitAfterValue, "test2", SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
        memcpy(value, &config, blobSize);
        return blobSize;
      });
  EXPECT_CALL(
      config,
      getBlob(
          StrEq("0_gpm_meter"),
          _,
          sizeof(Supla::Sensor::GeneralPurposeMeter::GPMMeterSpecificConfig)))
      .Times(1)
      .WillOnce([](const char *key, char *value, size_t blobSize) {
        Supla::Sensor::GeneralPurposeMeter::GPMMeterSpecificConfig config = {};
        EXPECT_EQ(blobSize, sizeof(config));
        config.counterType = 15;
        config.fillMissingData = 16;
        config.includeValueAddedInHistory = 17;
        memcpy(value, &config, blobSize);
        return blobSize;
      });
  EXPECT_CALL(config, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)));


  gp.setDefaultValueAdded(1);
  gp.setDefaultValueDivider(2);
  gp.setDefaultValueMultiplier(3);
  gp.setDefaultValuePrecision(4);
  gp.setDefaultUnitBeforeValue("before");
  gp.setDefaultUnitAfterValue("after");

  EXPECT_EQ(gp.getValueAdded(), 0);
  EXPECT_EQ(gp.getValueDivider(), 0);
  EXPECT_EQ(gp.getValueMultiplier(), 0);
  EXPECT_EQ(gp.getValuePrecision(), 0);

  char testString[100] = {};
  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "");

  gp.onLoadConfig(nullptr);
  gp.onInit();

  EXPECT_EQ(gp.getValueAdded(), 11);
  EXPECT_EQ(gp.getValueDivider(), 12);
  EXPECT_EQ(gp.getValueMultiplier(), 13);
  EXPECT_EQ(gp.getValuePrecision(), 14);

  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "test1");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "test2");
}

TEST_F(GpMeterTestsFixture, handleChannelConfigCheck) {
  Supla::Sensor::GeneralPurposeMeter gp;
  ConfigMock cfg;

  EXPECT_CALL(cfg, saveWithDelay(_)).Times(2);
  EXPECT_CALL(
      cfg,
      setBlob(
          _,
          _,
          sizeof(Supla::Sensor::GeneralPurposeChannelBase::GPMCommonConfig)))
      .Times(1)
      .WillOnce(Return(
          sizeof(Supla::Sensor::GeneralPurposeChannelBase::GPMCommonConfig)));
  EXPECT_CALL(
      cfg,
      setBlob(
          _,
          _,
          sizeof(Supla::Sensor::GeneralPurposeMeter::GPMMeterSpecificConfig)))
      .Times(1)
      .WillOnce(Return(
          sizeof(Supla::Sensor::GeneralPurposeMeter::GPMMeterSpecificConfig)));
  EXPECT_CALL(cfg, setUInt8(_, _)).Times(0);

  gp.onInit();

  TSD_ChannelConfig config = {};
  auto gpConfig = reinterpret_cast<TChannelConfig_GeneralPurposeMeter *>(
      config.Config);
  config.ChannelNumber = 0;
  config.ConfigSize = sizeof(TChannelConfig_GeneralPurposeMeter);
  config.Func = SUPLA_CHANNELFNC_GENERAL_PURPOSE_METER;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  gpConfig->ChartType = 1;
  gpConfig->RefreshIntervalMs = 1000;
  gpConfig->NoSpaceBeforeValue = 2;
  gpConfig->NoSpaceAfterValue = 3;
  gpConfig->KeepHistory = 4;
  gpConfig->ValueAdded = 5;
  gpConfig->ValueDivider = 6;
  gpConfig->ValueMultiplier = 7;
  gpConfig->ValuePrecision = 8;
  gpConfig->CounterType = 9;
  gpConfig->IncludeValueAddedInHistory = 10;
  gpConfig->FillMissingData = 11;
  strncpy(gpConfig->UnitBeforeValue, "before", SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  strncpy(gpConfig->UnitAfterValue, "after", SUPLA_GENERAL_PURPOSE_UNIT_SIZE);

  gp.handleChannelConfig(&config, false);

  EXPECT_EQ(gp.getChartType(), 1);
  EXPECT_EQ(gp.getRefreshIntervalMs(), 1000);
  EXPECT_EQ(gp.getNoSpaceBeforeValue(), 2);
  EXPECT_EQ(gp.getNoSpaceAfterValue(), 3);
  EXPECT_EQ(gp.getKeepHistory(), 4);
  EXPECT_EQ(gp.getValueAdded(), 5);
  EXPECT_EQ(gp.getValueDivider(), 6);
  EXPECT_EQ(gp.getValueMultiplier(), 7);
  EXPECT_EQ(gp.getValuePrecision(), 8);
  EXPECT_EQ(gp.getCounterType(), 9);
  EXPECT_EQ(gp.getIncludeValueAddedInHistory(), 10);
  EXPECT_EQ(gp.getFillMissingData(), 11);

  EXPECT_EQ(gp.getDefaultValueAdded(), 0);
  EXPECT_EQ(gp.getDefaultValueDivider(), 0);
  EXPECT_EQ(gp.getDefaultValueMultiplier(), 0);
  EXPECT_EQ(gp.getDefaultValuePrecision(), 0);

  char testString[100] = {};
  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "before");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "after");

  gp.getDefaultUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getDefaultUnitAfterValue(testString);
  EXPECT_STREQ(testString, "");
}

// When server sends config and default values differ from GPM defaults,
// then GPM applies config, but not default values. Then it sets config change
// flag which will result in sending config change to server.
TEST_F(GpMeterTestsFixture, handleChannelConfigWithInvalidDefaults) {
  Supla::Sensor::GeneralPurposeMeter gp;
  ConfigMock cfg;

  EXPECT_CALL(cfg, saveWithDelay(_)).Times(3);
  EXPECT_CALL(
      cfg,
      setBlob(
          _,
          _,
          sizeof(Supla::Sensor::GeneralPurposeChannelBase::GPMCommonConfig)))
      .Times(1)
      .WillOnce(Return(
          sizeof(Supla::Sensor::GeneralPurposeChannelBase::GPMCommonConfig)));
  EXPECT_CALL(
      cfg,
      setBlob(
          _,
          _,
          sizeof(Supla::Sensor::GeneralPurposeMeter::GPMMeterSpecificConfig)))
      .Times(1)
      .WillOnce(Return(
          sizeof(Supla::Sensor::GeneralPurposeMeter::GPMMeterSpecificConfig)));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), 1))
      .Times(1)
      .WillOnce(Return(1));

  gp.onInit();

  TSD_ChannelConfig config = {};
  auto gpConfig = reinterpret_cast<TChannelConfig_GeneralPurposeMeter *>(
      config.Config);
  config.ChannelNumber = 0;
  config.ConfigSize = sizeof(TChannelConfig_GeneralPurposeMeter);
  config.Func = SUPLA_CHANNELFNC_GENERAL_PURPOSE_METER;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  gpConfig->ChartType = 1;
  gpConfig->RefreshIntervalMs = 1000;
  gpConfig->NoSpaceBeforeValue = 2;
  gpConfig->NoSpaceAfterValue = 3;
  gpConfig->KeepHistory = 4;
  gpConfig->ValueAdded = 5;
  gpConfig->ValueDivider = 6;
  gpConfig->ValueMultiplier = 7;
  gpConfig->ValuePrecision = 8;
  gpConfig->CounterType = 9;
  gpConfig->IncludeValueAddedInHistory = 10;
  gpConfig->FillMissingData = 11;
  strncpy(gpConfig->UnitBeforeValue, "before", SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  strncpy(gpConfig->UnitAfterValue, "after", SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  // Invalid defaults from server
  gpConfig->DefaultValueAdded = 9;

  gp.handleChannelConfig(&config, false);

  EXPECT_EQ(gp.getChartType(), 1);
  EXPECT_EQ(gp.getRefreshIntervalMs(), 1000);
  EXPECT_EQ(gp.getNoSpaceBeforeValue(), 2);
  EXPECT_EQ(gp.getNoSpaceAfterValue(), 3);
  EXPECT_EQ(gp.getKeepHistory(), 4);
  EXPECT_EQ(gp.getValueAdded(), 5);
  EXPECT_EQ(gp.getValueDivider(), 6);
  EXPECT_EQ(gp.getValueMultiplier(), 7);
  EXPECT_EQ(gp.getValuePrecision(), 8);
  EXPECT_EQ(gp.getCounterType(), 9);
  EXPECT_EQ(gp.getIncludeValueAddedInHistory(), 10);
  EXPECT_EQ(gp.getFillMissingData(), 11);

  EXPECT_EQ(gp.getDefaultValueAdded(), 0);  // not changed
  EXPECT_EQ(gp.getDefaultValueDivider(), 0);
  EXPECT_EQ(gp.getDefaultValueMultiplier(), 0);
  EXPECT_EQ(gp.getDefaultValuePrecision(), 0);

  char testString[100] = {};
  gp.getUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "before");

  gp.getUnitAfterValue(testString);
  EXPECT_STREQ(testString, "after");

  gp.getDefaultUnitBeforeValue(testString);
  EXPECT_STREQ(testString, "");

  gp.getDefaultUnitAfterValue(testString);
  EXPECT_STREQ(testString, "");
}

TEST_F(GpMeterTestsFixture, localConfigChangeShouldBeSavedAndSend) {
  Supla::Sensor::GeneralPurposeMeter gp;
  ConfigMock cfg;

  EXPECT_CALL(cfg, saveWithDelay(_)).Times(4);
  EXPECT_CALL(
      cfg,
      setBlob(
           _,
           _,
           sizeof(Supla::Sensor::GeneralPurposeChannelBase::GPMCommonConfig)))
      .Times(1)
      .WillOnce(Return(
          sizeof(Supla::Sensor::GeneralPurposeChannelBase::GPMCommonConfig)));
  EXPECT_CALL(
      cfg,
      setBlob(
          _,
          _,
          sizeof(Supla::Sensor::GeneralPurposeMeter::GPMMeterSpecificConfig)))
      .Times(1)
      .WillOnce(Return(
          sizeof(Supla::Sensor::GeneralPurposeMeter::GPMMeterSpecificConfig)));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), 1))
      .Times(2)
      .WillOnce(Return(1))
      .WillOnce(Return(1));

  gp.onInit();

  gp.setValueMultiplier(10);
  gp.setCounterType(2);

  EXPECT_EQ(gp.getValueMultiplier(), 10);
  EXPECT_EQ(gp.getCounterType(), 2);
}

TEST_F(GpMeterTestsFixture, byDefaultVirtualMode) {
  Supla::Sensor::GeneralPurposeMeter gp;
  SimpleTime time;

  gp.setValue(42);
  gp.onInit();

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 42);

  gp.setValue(100);
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 42);

  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 42);

  time.advance(6000);
  gp.iterateAlways();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 100);
}
