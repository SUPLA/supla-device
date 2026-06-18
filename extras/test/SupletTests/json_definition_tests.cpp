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
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/storage/config.h>
#include <supla/suplet/json_definition.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/runtime.h>
#include <simple_time.h>

namespace {

class NoopConfig : public Supla::Config {
 public:
  bool init() override { return true; }
  void removeAll() override {}
  bool setString(const char *, const char *) override { return false; }
  bool getString(const char *, char *, size_t) override { return false; }
  int getStringSize(const char *) override { return -1; }
  bool setBlob(const char *, const char *, size_t) override { return true; }
  bool getBlob(const char *, char *, size_t) override { return false; }
  int getBlobSize(const char *) override { return -1; }
  bool getInt8(const char *, int8_t *) override { return false; }
  bool getUInt8(const char *, uint8_t *) override { return false; }
  bool getInt32(const char *, int32_t *) override { return false; }
  bool getUInt32(const char *, uint32_t *) override { return false; }
  bool setInt8(const char *, const int8_t) override { return false; }
  bool setUInt8(const char *, const uint8_t) override { return true; }
  bool setInt32(const char *, const int32_t) override { return false; }
  bool setUInt32(const char *, const uint32_t) override { return false; }
  bool eraseKey(const char *) override { return true; }
};

class SupletJsonFixture : public testing::Test {
 protected:
  SimpleTime time;

  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    while (Supla::Element::begin() != nullptr) {
      delete Supla::Element::begin();
    }
    Supla::Channel::resetToDefaults();
  }
};

}  // namespace

TEST(SupletJsonDefinitionTests, ParsesVirtualDefinition) {
  const char json[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":2,"
      "\"definitionId\":1001,"
      "\"definitionVersion\":7,"
      "\"maxInstances\":6,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"name\":\"Virtual controls\","
      "\"handlerConfig\":{\"ignored\":\"for now\"},"
      "\"parameters\":["
      "{\"key\":\"relay.count\",\"type\":\"uint8\",\"default\":4,"
      "\"min\":1,\"max\":16,\"lifecycle\":\"createOnly\","
      "\"affectsTopology\":true},"
      "{\"key\":\"mode\",\"type\":\"enum\",\"default\":\"avg\","
      "\"values\":[\"avg\",\"min\",\"max\"],\"required\":true},"
      "{\"key\":\"password\",\"type\":\"secret\","
      "\"lifecycle\":\"secret\"}"
      "],"
      "\"channels\":["
      "{\"key\":\"relay_main\",\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\",\"caption\":\"Main relay\"},"
      "{\"key\":\"door\",\"kind\":\"virtualBinarySensor\","
      "\"function\":\"openingSensorDoor\"},"
      "{\"key\":\"temp\",\"kind\":\"virtualThermometer\","
      "\"function\":\"thermometer\"}"
      "]"
      "}";

  Supla::Suplet::JsonDefinition parsed;
  ASSERT_TRUE(Supla::Suplet::JsonDefinitionParser::parse(json, &parsed));

  const auto *definition = parsed.getDefinition();
  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->schemaVersion, 1);
  EXPECT_EQ(definition->handlerVersion, 2);
  EXPECT_EQ(definition->definitionId, 1001u);
  EXPECT_EQ(definition->definitionVersion, 7u);
  EXPECT_EQ(definition->maxInstances, 6);
  EXPECT_EQ(definition->category, Supla::Suplet::Category::Virtual);
  EXPECT_EQ(definition->kind, Supla::Suplet::Kind::VirtualRelay);
  EXPECT_STREQ(definition->name, "Virtual controls");
  ASSERT_EQ(definition->parameterCount, 3);
  EXPECT_STREQ(definition->parameters[0].key, "relay.count");
  EXPECT_EQ(definition->parameters[0].type,
            Supla::Suplet::ParameterType::UInt8);
  EXPECT_EQ(definition->parameters[0].lifecycle,
            Supla::Suplet::ParameterLifecycle::CreateOnly);
  EXPECT_EQ(definition->parameters[0].defaultNumber, 4);
  EXPECT_EQ(definition->parameters[0].min, 1);
  EXPECT_EQ(definition->parameters[0].max, 16);
  EXPECT_EQ(definition->parameters[0].affectsTopology, 1);
  EXPECT_STREQ(definition->parameters[1].key, "mode");
  EXPECT_EQ(definition->parameters[1].type,
            Supla::Suplet::ParameterType::Enum);
  EXPECT_STREQ(definition->parameters[1].defaultText, "avg");
  EXPECT_STREQ(definition->parameters[1].enumValues, "avg,min,max");
  EXPECT_EQ(definition->parameters[1].required, 1);
  EXPECT_EQ(definition->parameters[2].type,
            Supla::Suplet::ParameterType::Secret);
  EXPECT_EQ(definition->parameters[2].lifecycle,
            Supla::Suplet::ParameterLifecycle::Secret);
  ASSERT_EQ(definition->channelCount, 3);

  EXPECT_EQ(definition->channels[0].channelKey,
            Supla::Suplet::channelKeyFromString("relay_main"));
  EXPECT_EQ(definition->channels[0].kind,
            Supla::Suplet::ChannelKind::VirtualRelay);
  EXPECT_EQ(definition->channels[0].defaultFunction,
            SUPLA_CHANNELFNC_POWERSWITCH);
  EXPECT_STREQ(definition->channels[0].caption, "Main relay");
  EXPECT_EQ(definition->channels[1].kind,
            Supla::Suplet::ChannelKind::VirtualBinarySensor);
  EXPECT_EQ(definition->channels[2].kind,
            Supla::Suplet::ChannelKind::VirtualThermometer);
}

TEST(SupletJsonDefinitionTests, ParsesVersionedParameterizedRelayDefinition) {
  const char json[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":2011,"
      "\"definitionVersion\":2,"
      "\"maxInstances\":3,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"parameters\":["
      "{\"key\":\"relay.count\",\"type\":\"uint8\",\"default\":1,"
      "\"min\":1,\"max\":4,\"lifecycle\":\"createOnly\","
      "\"affectsTopology\":true},"
      "{\"key\":\"mode\",\"type\":\"enum\",\"default\":\"avg\","
      "\"values\":[\"avg\",\"min\",\"max\"],\"required\":true},"
      "{\"key\":\"host\",\"type\":\"string\",\"required\":true}"
      "],"
      "\"channels\":[{"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"Param relay@@@\""
      "}]"
      "}";

  Supla::Suplet::JsonDefinition parsed;
  ASSERT_TRUE(Supla::Suplet::JsonDefinitionParser::parse(json, &parsed));

  const auto *definition = parsed.getDefinition();
  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->definitionId, 2011u);
  EXPECT_EQ(definition->definitionVersion, 2u);
  EXPECT_EQ(definition->maxInstances, 3);
  ASSERT_EQ(definition->parameterCount, 3);
  ASSERT_EQ(definition->channelCount, 1);
  EXPECT_STREQ(definition->channels[0].caption, "Param relay@@@");
}

TEST(SupletJsonDefinitionTests, RejectsInvalidDefinitions) {
  Supla::Suplet::JsonDefinition parsed;

  EXPECT_FALSE(Supla::Suplet::JsonDefinitionParser::parse(
      "{\"definitionId\":1,\"definitionVersion\":1,"
      "\"category\":\"virtual\",\"kind\":\"virtualRelay\"}",
      &parsed));
  EXPECT_FALSE(Supla::Suplet::JsonDefinitionParser::parse(
      "{\"definitionId\":1,\"definitionVersion\":1,"
      "\"kind\":\"virtualRelay\","
      "\"channels\":[{\"key\":\"a\",\"kind\":\"virtualRelay\"}]}",
      &parsed));
  EXPECT_FALSE(Supla::Suplet::JsonDefinitionParser::parse(
      "{\"definitionId\":1,\"definitionVersion\":1,"
      "\"category\":\"virtual\","
      "\"channels\":[{\"key\":\"a\",\"kind\":\"virtualRelay\"}]}",
      &parsed));
  EXPECT_FALSE(Supla::Suplet::JsonDefinitionParser::parse(
      "{\"definitionId\":1,\"definitionVersion\":1,"
      "\"category\":\"virtual\",\"kind\":\"virtualRelay\","
      "\"channels\":["
      "{\"key\":\"same\",\"kind\":\"virtualRelay\"},"
      "{\"key\":\"same\",\"kind\":\"virtualRelay\"}]}",
      &parsed));
  EXPECT_FALSE(Supla::Suplet::JsonDefinitionParser::parse(
      "{\"definitionId\":1,\"definitionVersion\":1,"
      "\"category\":\"bad\",\"kind\":\"virtualRelay\","
      "\"channels\":[{\"key\":\"a\",\"kind\":\"virtualRelay\"}]}",
      &parsed));
  EXPECT_FALSE(Supla::Suplet::JsonDefinitionParser::parse(
      "{\"definitionId\":1,\"definitionVersion\":1,"
      "\"maxInstances\":0,"
      "\"category\":\"virtual\",\"kind\":\"virtualRelay\","
      "\"channels\":[{\"key\":\"a\",\"kind\":\"virtualRelay\"}]}",
      &parsed));
}

TEST_F(SupletJsonFixture, CreatesRuntimeElementsFromJsonDefinition) {
  const char json[] =
      "{"
      "\"definitionId\":2002,"
      "\"definitionVersion\":1,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"channels\":["
      "{\"key\":\"relay\",\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\"},"
      "{\"key\":\"temperature\",\"kind\":\"virtualThermometer\","
      "\"function\":\"thermometer\"}"
      "]"
      "}";
  Supla::Suplet::JsonDefinition parsed;
  ASSERT_TRUE(Supla::Suplet::JsonDefinitionParser::parse(json, &parsed));

  NoopConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ChannelAllocator occupied;
  ASSERT_TRUE(occupied.markOccupied(0));

  Supla::Suplet::InstanceRecord instance = {};
  instance.instanceId = 55;
  ASSERT_TRUE(manager.addInstanceFromDefinition(
      instance, *parsed.getDefinition(), occupied));
  const auto *record = manager.getInstanceTable()->findByInstanceId(55);
  ASSERT_NE(record, nullptr);

  Supla::Element *created[2] = {};
  ASSERT_TRUE(Supla::Suplet::Runtime::createElements(
      *parsed.getDefinition(), *record, created, 2));

  EXPECT_EQ(created[0]->getChannelNumber(), 1);
  EXPECT_EQ(created[1]->getChannelNumber(), 2);
  EXPECT_EQ(created[0]->getChannel()->getSubDeviceId(), record->subDeviceId);
  EXPECT_EQ(created[1]->getChannel()->getSubDeviceId(), record->subDeviceId);

  auto thermometer =
      reinterpret_cast<Supla::Sensor::VirtualThermometer *>(created[1]);
  thermometer->setValue(23.5);
  EXPECT_DOUBLE_EQ(thermometer->getValue(), 23.5);
}
