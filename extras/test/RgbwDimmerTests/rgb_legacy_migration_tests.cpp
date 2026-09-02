// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla/control/dimmer_base.h>
#include <supla/control/rgb_base.h>
#include <supla/control/rgbw_base.h>

class RgbLegacyMigrationRgbBase : public Supla::Control::RGBBase {
 public:
  void setRGBWValueOnDevice(uint32_t, uint32_t, uint32_t, uint32_t) override {}
};

class RgbLegacyMigrationRgbwBase : public Supla::Control::RGBWBase {
 public:
  void setRGBWValueOnDevice(uint32_t, uint32_t, uint32_t, uint32_t) override {}
};

class RgbLegacyMigrationDimmerBase : public Supla::Control::DimmerBase {
 public:
  void setRGBWValueOnDevice(uint32_t, uint32_t, uint32_t, uint32_t) override {}
};

TEST(RgbLegacyMigrationTests, RgbBaseNeedsMigration) {
  RgbLegacyMigrationRgbBase rgb;
  EXPECT_FALSE(rgb.isStateStorageMigrationNeeded());
}

TEST(RgbLegacyMigrationTests, RgbwBaseNeedsMigration) {
  RgbLegacyMigrationRgbwBase rgbw;
  EXPECT_FALSE(rgbw.isStateStorageMigrationNeeded());
}

TEST(RgbLegacyMigrationTests, DimmerBaseNeedsMigration) {
  RgbLegacyMigrationDimmerBase dimmer;
  EXPECT_FALSE(dimmer.isStateStorageMigrationNeeded());
}
