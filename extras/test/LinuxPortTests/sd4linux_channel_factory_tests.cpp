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
#include <linux_channel_factory.h>

namespace {

class Sd4linuxChannelFactoryTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Linux::ChannelFactoryRegistry::instance().clear();
  }

  void TearDown() override {
    Supla::Linux::ChannelFactoryRegistry::instance().clear();
  }
};

}  // namespace

TEST_F(Sd4linuxChannelFactoryTests, RegistersFactoryByPluginAndType) {
  auto& registry = Supla::Linux::ChannelFactoryRegistry::instance();

  EXPECT_TRUE(registry.registerFactory(
      "test_plugin",
      "test_channel",
      [](const Supla::Linux::ChannelFactoryContext&) { return true; }));

  auto factory = registry.findByType("test_channel");
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->pluginName, "test_plugin");
  EXPECT_EQ(factory->typeName, "test_channel");
  EXPECT_TRUE(registry.hasPlugin("test_plugin"));
  EXPECT_FALSE(registry.hasPlugin("missing_plugin"));
  EXPECT_EQ(registry.findByType("missing_channel"), nullptr);
}

TEST_F(Sd4linuxChannelFactoryTests, RejectsDuplicateChannelType) {
  auto& registry = Supla::Linux::ChannelFactoryRegistry::instance();

  EXPECT_TRUE(registry.registerFactory(
      "plugin_a",
      "shared_type",
      [](const Supla::Linux::ChannelFactoryContext&) { return true; }));
  EXPECT_FALSE(registry.registerFactory(
      "plugin_b",
      "shared_type",
      [](const Supla::Linux::ChannelFactoryContext&) { return true; }));

  auto factory = registry.findByType("shared_type");
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->pluginName, "plugin_a");
}

TEST_F(Sd4linuxChannelFactoryTests, RejectsInvalidRegistration) {
  auto& registry = Supla::Linux::ChannelFactoryRegistry::instance();

  EXPECT_FALSE(registry.registerFactory(
      "",
      "type",
      [](const Supla::Linux::ChannelFactoryContext&) { return true; }));
  EXPECT_FALSE(registry.registerFactory(
      "plugin",
      "",
      [](const Supla::Linux::ChannelFactoryContext&) { return true; }));
  EXPECT_FALSE(registry.registerFactory("plugin", "type", nullptr));
}
