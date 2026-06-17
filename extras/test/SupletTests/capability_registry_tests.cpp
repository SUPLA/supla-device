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
#include <supla/suplet/capability_registry.h>

namespace {

Supla::Suplet::Capability makeCapability(Supla::Suplet::Category category,
                                         Supla::Suplet::Kind kind,
                                         uint8_t handlerVersion = 1) {
  Supla::Suplet::Capability capability = {};
  capability.category = category;
  capability.kind = kind;
  capability.minSchemaVersion = 1;
  capability.maxSchemaVersion = 2;
  capability.handlerVersion = handlerVersion;
  capability.maxInstances = 8;
  capability.supportsDownloadedDefinition = 1;
  capability.definitionId = 1234;
  capability.minDefinitionVersion = 1;
  capability.maxDefinitionVersion = 9;
  return capability;
}

}  // namespace

TEST(SupletCapabilityRegistryTests, AddsSupportedHandlerCapabilities) {
  Supla::Suplet::CapabilityRegistry registry;
  auto relay = makeCapability(Supla::Suplet::Category::Virtual,
                              Supla::Suplet::Kind::VirtualRelay);
  auto aggregate = makeCapability(Supla::Suplet::Category::Aggregate,
                                  Supla::Suplet::Kind::ThermometerGroup);

  EXPECT_TRUE(registry.add(relay));
  EXPECT_TRUE(registry.add(aggregate));
  EXPECT_FALSE(registry.add(relay));
  EXPECT_EQ(registry.getCount(), 2);

  Supla::Suplet::Capability capability = {};
  ASSERT_TRUE(registry.getCapability(0, &capability));
  EXPECT_EQ(capability.category, Supla::Suplet::Category::Virtual);
  EXPECT_EQ(capability.kind, Supla::Suplet::Kind::VirtualRelay);
  EXPECT_EQ(capability.minSchemaVersion, 1);
  EXPECT_EQ(capability.maxSchemaVersion, 2);
  EXPECT_EQ(capability.handlerVersion, 1);
  EXPECT_EQ(capability.maxInstances, 8);
  EXPECT_EQ(capability.supportsDownloadedDefinition, 1);
  EXPECT_EQ(capability.definitionId, 0u);
  EXPECT_EQ(capability.minDefinitionVersion, 0u);
  EXPECT_EQ(capability.maxDefinitionVersion, 0u);

  ASSERT_TRUE(registry.getCapability(1, &capability));
  EXPECT_EQ(capability.category, Supla::Suplet::Category::Aggregate);
  EXPECT_EQ(capability.kind, Supla::Suplet::Kind::ThermometerGroup);
  EXPECT_FALSE(registry.getCapability(2, &capability));
}

TEST(SupletCapabilityRegistryTests, RejectsInvalidCapabilities) {
  Supla::Suplet::CapabilityRegistry registry;
  auto valid = makeCapability(Supla::Suplet::Category::Virtual,
                              Supla::Suplet::Kind::VirtualRelay);
  auto invalid = valid;

  invalid.kind = Supla::Suplet::Kind::Unknown;
  EXPECT_FALSE(registry.add(invalid));

  invalid = valid;
  invalid.maxInstances = 0;
  EXPECT_FALSE(registry.add(invalid));

  invalid = valid;
  invalid.minSchemaVersion = 3;
  invalid.maxSchemaVersion = 2;
  EXPECT_FALSE(registry.add(invalid));

  EXPECT_TRUE(registry.add(valid));
  EXPECT_EQ(registry.getCount(), 1);
}
