// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
