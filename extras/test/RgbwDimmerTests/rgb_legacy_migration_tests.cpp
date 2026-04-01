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
