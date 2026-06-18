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
#include <supla/suplet/registry.h>

namespace {

Supla::Suplet::Definition makeDefinition(uint32_t id,
                                         uint32_t version,
                                         Supla::Suplet::Kind kind,
                                         Supla::Suplet::ChannelDefinition *ch) {
  ch->channelKey = id + 1000;
  ch->kind = Supla::Suplet::ChannelKind::VirtualRelay;
  Supla::Suplet::Definition definition = {};
  definition.definitionId = id;
  definition.definitionVersion = version;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = kind;
  definition.schemaVersion = 1;
  definition.handlerVersion = 2;
  definition.channels = ch;
  definition.channelCount = 1;
  return definition;
}

}  // namespace

TEST(SupletRegistryTests, AddsAndFindsDefinitions) {
  Supla::Suplet::ChannelDefinition ch1 = {};
  Supla::Suplet::ChannelDefinition ch2 = {};
  Supla::Suplet::ChannelDefinition ch3 = {};
  auto def1 = makeDefinition(10, 1, Supla::Suplet::Kind::VirtualRelay, &ch1);
  auto def2 =
      makeDefinition(20, 3, Supla::Suplet::Kind::VirtualBinarySensor, &ch2);
  auto def3 = makeDefinition(10, 2, Supla::Suplet::Kind::VirtualRelay, &ch3);
  Supla::Suplet::Registry registry;

  EXPECT_TRUE(registry.add(&def1, 4));
  EXPECT_TRUE(registry.add(&def2, 2, true));
  EXPECT_TRUE(registry.add(&def3, 4));
  EXPECT_FALSE(registry.add(&def1, 4));

  EXPECT_EQ(registry.getCount(), 3);
  EXPECT_EQ(registry.findDefinition(10), &def1);
  EXPECT_EQ(registry.findDefinition(10, 1), &def1);
  EXPECT_EQ(registry.findDefinition(10, 2), &def3);
  EXPECT_EQ(registry.findDefinition(20), &def2);
}

TEST(SupletRegistryTests, RejectsSameIdAndVersionForDifferentKind) {
  Supla::Suplet::ChannelDefinition relayChannel = {};
  Supla::Suplet::ChannelDefinition binaryChannel = {};
  auto relay = makeDefinition(
      10, 1, Supla::Suplet::Kind::VirtualRelay, &relayChannel);
  auto binary = makeDefinition(
      10, 1, Supla::Suplet::Kind::VirtualBinarySensor, &binaryChannel);
  Supla::Suplet::Registry registry;

  EXPECT_TRUE(registry.add(&relay, 4));
  EXPECT_FALSE(registry.add(&binary, 4));
  EXPECT_EQ(registry.getCount(), 1);
  EXPECT_EQ(registry.findDefinition(10, 1), &relay);
}

TEST(SupletRegistryTests, RemovesSingleVersionOrAllVersions) {
  Supla::Suplet::ChannelDefinition ch1 = {};
  Supla::Suplet::ChannelDefinition ch2 = {};
  auto def1 = makeDefinition(10, 1, Supla::Suplet::Kind::VirtualRelay, &ch1);
  auto def2 = makeDefinition(10, 2, Supla::Suplet::Kind::VirtualRelay, &ch2);
  Supla::Suplet::Registry registry;

  ASSERT_TRUE(registry.add(&def1, 4));
  ASSERT_TRUE(registry.add(&def2, 4));
  ASSERT_EQ(registry.getCount(), 2);

  EXPECT_TRUE(registry.remove(10, 2));
  EXPECT_EQ(registry.findDefinition(10, 1), &def1);
  EXPECT_EQ(registry.findDefinition(10, 2), nullptr);
  EXPECT_EQ(registry.getCount(), 1);

  ASSERT_TRUE(registry.add(&def2, 4));
  EXPECT_TRUE(registry.remove(10));
  EXPECT_EQ(registry.findDefinition(10, 1), nullptr);
  EXPECT_EQ(registry.findDefinition(10, 2), nullptr);
  EXPECT_EQ(registry.getCount(), 0);
}

TEST(SupletRegistryTests, ExposesCapabilities) {
  Supla::Suplet::ChannelDefinition ch = {};
  auto def = makeDefinition(30, 5, Supla::Suplet::Kind::VirtualRelay, &ch);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&def, 7, true));

  Supla::Suplet::Capability capability = {};
  ASSERT_TRUE(registry.getCapability(0, &capability));
  EXPECT_EQ(capability.category, Supla::Suplet::Category::Virtual);
  EXPECT_EQ(capability.kind, Supla::Suplet::Kind::VirtualRelay);
  EXPECT_EQ(capability.definitionId, 30u);
  EXPECT_EQ(capability.minDefinitionVersion, 5u);
  EXPECT_EQ(capability.maxDefinitionVersion, 5u);
  EXPECT_EQ(capability.minSchemaVersion, 1);
  EXPECT_EQ(capability.maxSchemaVersion, 1);
  EXPECT_EQ(capability.handlerVersion, 2);
  EXPECT_EQ(capability.maxInstances, 7);
  EXPECT_EQ(capability.supportsDownloadedDefinition, 1);
  EXPECT_FALSE(registry.getCapability(1, &capability));
}

TEST(SupletRegistryTests, RejectsInvalidDefinitionsAndCanRemove) {
  Supla::Suplet::ChannelDefinition ch = {};
  auto def = makeDefinition(40, 1, Supla::Suplet::Kind::VirtualRelay, &ch);
  Supla::Suplet::Definition invalid = def;
  invalid.definitionId = 0;
  Supla::Suplet::Registry registry;

  EXPECT_FALSE(registry.add(nullptr));
  EXPECT_FALSE(registry.add(&invalid));
  EXPECT_TRUE(registry.add(&def));
  EXPECT_TRUE(registry.remove(40));
  EXPECT_FALSE(registry.remove(40));
  EXPECT_EQ(registry.getCount(), 0);
}
