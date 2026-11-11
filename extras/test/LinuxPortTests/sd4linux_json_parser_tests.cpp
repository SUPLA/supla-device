// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <simple_time.h>
#include <supla/parser/json.h>
#include <supla/sensor/electricity_meter_parsed.h>
#include <supla/control/custom_relay.h>
#include <supla/payload/simple.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/sensor/container_parsed.h>
#include <supla/sensor/impulse_counter_parsed.h>
#include <supla/source/source.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

extern "C" const char *supla_test_get_last_log();
extern "C" void supla_test_clear_last_log();

namespace {

class FakeJsonSource : public Supla::Source::Source {
 public:
  std::string content;
  bool connected = true;
  int readCount = 0;

  std::string getContent() override {
    readCount++;
    return content;
  }

  bool isConnected() override {
    return connected;
  }
};

class Sd4linuxJsonParserTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    source.content.clear();
    source.connected = true;
    source.readCount = 0;
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }

  FakeJsonSource source;
};

class TestImpulseCounterParsed : public Supla::Sensor::ImpulseCounterParsed {
 public:
  explicit TestImpulseCounterParsed(Supla::Parser::Parser *parser)
      : Supla::Sensor::ImpulseCounterParsed(parser) {
  }

  TChannelConfig_ImpulseCounter getDefaultConfig() {
    TChannelConfig_ImpulseCounter config = {};
    int size = 0;
    fillChannelConfig(&config, &size, SUPLA_CONFIG_TYPE_DEFAULT);
    EXPECT_EQ(size, sizeof(TChannelConfig_ImpulseCounter));
    return config;
  }
};

union RawContainerValue {
  int8_t raw[SUPLA_CHANNELVALUE_SIZE];
  TContainerChannel_Value container;
};

}  // namespace

TEST_F(Sd4linuxJsonParserTests, RejectsNanStringValue) {
  SimpleTime time;
  source.content = R"({"counter":"nan"})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());
  EXPECT_TRUE(parser.isValid());

  const double value = parser.getValue("counter");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(parser.isValid());
}

TEST_F(Sd4linuxJsonParserTests, RejectsInfStringValue) {
  SimpleTime time;
  source.content = R"({"counter":"inf"})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());
  EXPECT_TRUE(parser.isValid());

  const double value = parser.getValue("counter");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(parser.isValid());
}

TEST_F(Sd4linuxJsonParserTests, RejectsOrdinaryInvalidStringValues) {
  SimpleTime time;
  source.content = R"({"counter":"abc"})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());
  EXPECT_TRUE(parser.isValid());

  const double value = parser.getValue("counter");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(parser.isValid());
}

TEST_F(Sd4linuxJsonParserTests, ParseErrorDoesNotLogJsonSource) {
  SimpleTime time;
  source.content = R"({"value":1,"password":"SUPER_SECRET_TEST_VALUE")";
  Supla::Parser::Json parser(&source);

  supla_test_clear_last_log();
  EXPECT_FALSE(parser.refreshParserSource());

  std::string log = supla_test_get_last_log();
  EXPECT_NE(log.find("JSON parsing error at byte"), std::string::npos);
  EXPECT_EQ(log.find("SUPER_SECRET_TEST_VALUE"), std::string::npos);
  EXPECT_EQ(log.find(source.content), std::string::npos);
}

TEST_F(Sd4linuxJsonParserTests, InvalidNumericValueDoesNotPoisonSharedParser) {
  SimpleTime time;
  source.content = R"({"state":"ready","temperature":"warm"})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());
  EXPECT_TRUE(parser.isValid());

  const double value = parser.getValue("temperature");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(parser.isValid());

  auto state = parser.getStateValue("state");
  ASSERT_TRUE(std::holds_alternative<std::string>(state));
  EXPECT_EQ(std::get<std::string>(state), "ready");
  EXPECT_TRUE(parser.isValid());
}

TEST_F(Sd4linuxJsonParserTests,
       InvalidNumericValueDoesNotMakeBinaryChannelOfflineOnCacheHit) {
  SimpleTime time;
  source.content = R"({"state":"ready","temperature":"warm"})";
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(5000);

  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");
  sensor.setOnValues({std::string("ready")});
  sensor.setUseOfflineOnInvalidState(true);

  time.advance(101);
  sensor.iterateAlways();

  ASSERT_TRUE(sensor.getChannel()->isStateOnline());
  ASSERT_EQ(source.readCount, 1);

  EXPECT_EQ(parser.getValue("temperature"), 0);
  ASSERT_FALSE(parser.isValid());
  ASSERT_TRUE(parser.isSourceValid());

  sensor.getChannel()->clearSendValue();

  time.advance(101);
  sensor.iterateAlways();

  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
  EXPECT_FALSE(sensor.getChannel()->isUpdateReady());
  EXPECT_EQ(source.readCount, 1);
}

TEST_F(Sd4linuxJsonParserTests, ReadsNestedSmartThingsCounterValues) {
  SimpleTime time;
  source.content =
      R"({"components":{"main":{)"
      R"("powerConsumptionReport":{"powerConsumption":{)"
      R"("value":{"energy":173300}}},)"
      R"("samsungce.waterConsumptionReport":{"waterConsumption":{)"
      R"("value":{"cumulativeAmount":21820900}}}}}})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());

  EXPECT_EQ(
      parser.getValue(
          "/components/main/powerConsumptionReport/powerConsumption/value/"
          "energy"),
      173300);
  EXPECT_EQ(
      parser.getValue(
          "/components/main/samsungce.waterConsumptionReport/waterConsumption/"
          "value/cumulativeAmount"),
      21820900);
}

TEST_F(Sd4linuxJsonParserTests,
       ImpulseCounterParsedKeepsRawCounterAndConfiguresImpulsesPerUnit) {
  SimpleTime time;
  source.content = R"({"energy":173300})";
  Supla::Parser::Json parser(&source);

  TestImpulseCounterParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Counter, "energy");
  sensor.setDefaultImpulsesPerUnit(1000);
  sensor.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_IC_ELECTRICITY_METER);

  sensor.onInit();

  EXPECT_EQ(sensor.getChannel()->getValueInt64(), 173300u);
  EXPECT_EQ(sensor.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_IC_ELECTRICITY_METER);

  auto config = sensor.getDefaultConfig();
  EXPECT_EQ(config.ImpulsesPerUnit, 1000);
}

TEST_F(Sd4linuxJsonParserTests, ImpulseCounterRejectsNanStringValue) {
  SimpleTime time;
  source.content = R"({"counter":"nan"})";
  Supla::Parser::Json parser(&source);

  Supla::Sensor::ImpulseCounterParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Counter, "counter");

  sensor.onInit();

  EXPECT_FALSE(parser.isValid());
  EXPECT_EQ(sensor.getChannel()->getValueInt64(), 0u);
}

TEST_F(Sd4linuxJsonParserTests,
       ContainerParsedInitialLevelUsesContainerValueEncoding) {
  SimpleTime time;
  for (const auto &[parsedLevel, expectedLevel] :
       std::vector<std::pair<int, int>>{{0, 0}, {50, 50}, {100, 100}}) {
    FakeJsonSource source;
    source.content = "{\"level\":" + std::to_string(parsedLevel) + "}";
    Supla::Parser::Json parser(&source);
    Supla::Sensor::ContainerParsed sensor(&parser);
    sensor.setMapping(Supla::Parser::Level, "level");

    sensor.onInit();

    RawContainerValue rawValue = {};
    sensor.getChannel()->fillRawValue(rawValue.raw);
    EXPECT_EQ(sensor.getChannel()->getContainerFillValue(), expectedLevel);
    EXPECT_EQ(rawValue.container.level, expectedLevel + 1);
    EXPECT_EQ(rawValue.container.flags, 0);
  }
}

TEST_F(Sd4linuxJsonParserTests,
       ContainerParsedInitialInvalidOrUnavailableLevelKeepsInvalidValue) {
  SimpleTime time;
  for (int parsedLevel : {-1, 101}) {
    FakeJsonSource source;
    source.content = "{\"level\":" + std::to_string(parsedLevel) + "}";
    Supla::Parser::Json parser(&source);
    Supla::Sensor::ContainerParsed sensor(&parser);
    sensor.setMapping(Supla::Parser::Level, "level");

    sensor.onInit();

    RawContainerValue rawValue = {};
    sensor.getChannel()->fillRawValue(rawValue.raw);
    EXPECT_EQ(sensor.getChannel()->getContainerFillValue(), -1);
    EXPECT_EQ(rawValue.container.level, 0);
    EXPECT_EQ(rawValue.container.flags, 0);
  }
}

TEST_F(Sd4linuxJsonParserTests,
       ContainerParsedInvalidLevelIsUnavailableAndLaterValidLevelRecovers) {
  SimpleTime time;
  FakeJsonSource source;
  source.content = R"({"level":"invalid"})";
  Supla::Parser::Json parser(&source);
  Supla::Sensor::ContainerParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Level, "level");
  sensor.setInternalLevelReporting(true);

  sensor.onInit();

  ASSERT_FALSE(parser.isValid());
  EXPECT_EQ(sensor.getChannel()->getContainerFillValue(), -1);
  RawContainerValue rawValue = {};
  sensor.getChannel()->fillRawValue(rawValue.raw);
  EXPECT_EQ(rawValue.container.level, 0);

  source.content = R"({"level":37})";
  time.advance(1001);
  sensor.iterateAlways();

  ASSERT_TRUE(parser.isValid());
  EXPECT_EQ(sensor.getChannel()->getContainerFillValue(), 37);
  sensor.getChannel()->fillRawValue(rawValue.raw);
  EXPECT_EQ(rawValue.container.level, 38);
}

TEST_F(Sd4linuxJsonParserTests,
       ContainerParsedDisconnectedSourceIsUnavailableAndRecovers) {
  SimpleTime time;
  FakeJsonSource source;
  source.content = R"({"level":63})";
  Supla::Parser::Json parser(&source);
  Supla::Sensor::ContainerParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Level, "level");
  sensor.setInternalLevelReporting(true);

  sensor.onInit();
  ASSERT_TRUE(parser.isValid());
  ASSERT_EQ(sensor.getChannel()->getContainerFillValue(), 63);

  source.connected = false;
  time.advance(1001);
  sensor.iterateAlways();

  ASSERT_TRUE(parser.isValid());
  EXPECT_FALSE(sensor.getChannel()->isStateOnline());
  EXPECT_EQ(sensor.getChannel()->getContainerFillValue(), -1);
  RawContainerValue rawValue = {};
  sensor.getChannel()->fillRawValue(rawValue.raw);
  EXPECT_EQ(rawValue.container.level, 0);

  source.connected = true;
  source.content = R"({"level":24})";
  time.advance(1001);
  sensor.iterateAlways();

  EXPECT_TRUE(parser.isValid());
  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
  EXPECT_EQ(sensor.getChannel()->getContainerFillValue(), 24);
  sensor.getChannel()->fillRawValue(rawValue.raw);
  EXPECT_EQ(rawValue.container.level, 25);
}

TEST_F(Sd4linuxJsonParserTests,
       ContainerParsedInitialLevelPreservesContainerFlags) {
  SimpleTime time;
  FakeJsonSource source;
  source.content = R"({"level":50})";
  Supla::Parser::Json parser(&source);
  Supla::Sensor::ContainerParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Level, "level");

  auto channel = sensor.getChannel();
  channel->setContainerWarning(true);
  channel->setContainerAlarm(true);
  channel->setContainerInvalidSensorState(true);
  channel->setContainerSoundAlarmOn(true);

  sensor.onInit();

  RawContainerValue rawValue = {};
  channel->fillRawValue(rawValue.raw);
  EXPECT_EQ(channel->getContainerFillValue(), 50);
  EXPECT_EQ(rawValue.container.level, 51);
  EXPECT_EQ(rawValue.container.flags,
            CONTAINER_FLAG_WARNING_LEVEL | CONTAINER_FLAG_ALARM_LEVEL |
                CONTAINER_FLAG_INVALID_SENSOR_STATE |
                CONTAINER_FLAG_SOUND_ALARM_ON);
  EXPECT_TRUE(channel->isContainerWarningActive());
  EXPECT_TRUE(channel->isContainerAlarmActive());
  EXPECT_TRUE(channel->isContainerInvalidSensorStateActive());
  EXPECT_TRUE(channel->isContainerSoundAlarmOn());
}

namespace {
class DocumentSource : public Supla::Source::Source {
 public:
  std::vector<Supla::Source::Document> documents = {
      {"0508", "", false, true}, {"0505", "", false, true}};
  std::string getContent() override { return "must not be parsed"; }
  std::vector<Supla::Source::Document> getDocuments() override {
    return documents;
  }
  std::vector<std::string> getDocumentIds() const override {
    return {"0508", "0505"};
  }
};
}  // namespace

TEST_F(Sd4linuxJsonParserTests, MultipleDocumentsRecoverWithoutMixingValues) {
  SimpleTime time;
  DocumentSource source;
  Supla::Parser::Json parser(&source, true);
  parser.addKey("0505:/data/val", -1);
  parser.addKey("0508:/data/val", -1);
  EXPECT_FALSE(parser.refreshParserSource());
  EXPECT_FALSE(parser.isSourceValid());
  EXPECT_EQ(parser.getValue("0505:/data/val"), 0);
  EXPECT_FALSE(parser.isValid());
  source.documents[1] = {"0505", R"({"data":{"val":"230.5"}})", true, true};
  EXPECT_FALSE(parser.refreshSource());
  source.documents[0] = {"0508", R"({"data":{"val":4.2}})", true, true};
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("0505:/data/val"), 230.5);
  EXPECT_DOUBLE_EQ(parser.getValue("0508:/data/val"), 4.2);
  std::swap(source.documents[0], source.documents[1]);
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("0505:/data/val"), 230.5);
  source.documents[1].content = "invalid";
  EXPECT_FALSE(parser.refreshSource());
  EXPECT_FALSE(parser.isSourceValid());
  EXPECT_EQ(parser.getValue("0505:/data/val"), 0);
  EXPECT_FALSE(parser.isValid());
  source.documents[1].content = R"({"data":{"val":"5.1"}})";
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("0508:/data/val"), 5.1);
  EXPECT_DOUBLE_EQ(parser.getValue("0505:/data/val"), 230.5);
  source.documents[1].fresh = false;
  EXPECT_FALSE(parser.refreshSource());
  source.documents[1].fresh = true;
  source.documents[1].content = "";
  EXPECT_FALSE(parser.refreshSource());
  source.documents.pop_back();
  EXPECT_FALSE(parser.refreshSource());
}

TEST_F(Sd4linuxJsonParserTests, MultipleDocumentPointersStatesAndValidation) {
  DocumentSource source;
  source.documents[0] = {"0508", R"({"data":{"val":4.2}})", true, true};
  source.documents[1] = {"0505",
      R"({"a/b":{"~nested":[{"val":"231.5"}]},"data":{"val":"ON"}})",
      true, true};
  Supla::Parser::Json parser(&source, true);
  parser.addKey("0505:/a~1b/~0nested/0/val", -1);
  parser.addKey("0508:/data/val", -1);
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("0505:/a~1b/~0nested/0/val"), 231.5);
  EXPECT_EQ(std::get<std::string>(parser.getStateValue("0505:/data/val")),
            "ON");
  EXPECT_EQ(parser.getValue("0508:/missing"), 0);
  EXPECT_FALSE(parser.isValid());
  EXPECT_TRUE(parser.isSourceValid());
  EXPECT_DOUBLE_EQ(parser.getValue("0508:/data/val"), 4.2);
  EXPECT_TRUE(parser.isValid());
  EXPECT_EQ(parser.getValue("505:/data/val"), 0);
  EXPECT_FALSE(parser.isValid());
  for (const auto* key : {"505:/data/val", "0505:data/val", ":/val",
                          "0505:/bad~2escape", "0505"}) {
    EXPECT_THROW(parser.addKey(key, -1), std::invalid_argument);
  }
}

TEST_F(Sd4linuxJsonParserTests, ExplicitSingleDocumentModeKeepsLiteralKeys) {
  source.content = R"({"0505:/data/val":"12.5","data":{"val":"230.5"}})";
  Supla::Parser::Json parser(&source, false);
  parser.addKey("0505:/data/val", -1);
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("0505:/data/val"), 12.5);
  EXPECT_DOUBLE_EQ(parser.getValue("/data/val"), 230.5);
  EXPECT_EQ(source.getDocumentIds(), std::vector<std::string>{"default"});
  EXPECT_EQ(source.getDocuments()[0].content, source.content);
}

TEST_F(Sd4linuxJsonParserTests, SingleJsonMeterPreservesUnitsAndMultipliers) {
  SimpleTime time;
  source.content = R"({"voltage":"230.5","current":4200,"energy":12.345})";
  Supla::Parser::Json parser(&source);
  Supla::Sensor::ElectricityMeterParsed meter(&parser);
  meter.setMapping(Supla::Parser::Voltage1, "voltage");
  meter.setMapping(Supla::Parser::Current1, "current");
  meter.setMultiplier(Supla::Parser::Current1, 0.001);
  meter.setMapping(Supla::Parser::FwdActEnergy1, "energy");
  meter.onInit();
  EXPECT_EQ(meter.getVoltage(0), 23050);
  EXPECT_EQ(meter.getCurrent(0), 4200u);
  EXPECT_EQ(meter.getFwdActEnergy(0), 1234500u);
  source.content = R"({"voltage":"invalid","current":0,"energy":0})";
  time.advance(5001);
  meter.readValuesFromDevice();
  meter.updateChannelValues();
  const auto* value = reinterpret_cast<TElectricityMeter_ExtendedValue_V3*>(
      meter.getChannel()->getExtValue()->value);
  EXPECT_EQ(value->m_count, 0);
  EXPECT_FALSE(parser.isValid());
  EXPECT_TRUE(parser.isSourceValid());
  EXPECT_EQ(meter.getFwdActEnergy(0), 1234500u);
}

namespace {
class CapturingJsonOutput : public Supla::Output::Output {
 public:
  std::string content;
  bool putContent(int) override { return false; }
  bool putContent(bool) override { return false; }
  bool putContent(const std::vector<int>&) override { return false; }
  bool putContent(const std::string& value) override {
    content = value;
    return true;
  }
};
}  // namespace

TEST_F(Sd4linuxJsonParserTests,
       CustomRelayReadsNestedStateAndSendsExactSimpleJson) {
  SimpleTime time;
  source.content = R"({"data":{"val":"ON"}})";
  Supla::Parser::Json parser(&source);
  CapturingJsonOutput output;
  Supla::Payload::Simple payload(&output);
  Supla::Control::CustomRelay relay(&parser, &payload);
  relay.Supla::Sensor::SensorParsedBase::setMapping(Supla::Parser::State,
                                                  "/data/val");
  relay.setOnValues({std::string("ON")});
  relay.setSetOnValue(std::string(R"({"data":{"val":"ON"}})"));
  relay.setSetOffValue(std::string(R"({"data":{"val":"OFF"}})"));
  EXPECT_TRUE(relay.isOn());
  relay.turnOn();
  EXPECT_EQ(output.content, R"({"data":{"val":"ON"}})");
  relay.turnOff();
  EXPECT_EQ(output.content, R"({"data":{"val":"OFF"}})");
}

TEST_F(Sd4linuxJsonParserTests, OnlyRequiredDocumentsDetermineSourceValidity) {
  DocumentSource source;
  source.documents[0] = {"0508", "broken-unused-document", true, true};
  source.documents[1] = {"0505", R"({"data":{"val":230}})", true, true};
  Supla::Parser::Json parser(&source, true);
  parser.addKey("0505:/data/val", -1);
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("0505:/data/val"), 230);
  for (const auto* content : {"", "invalid", "{", R"({"data":{"val":1e999}})"}) {
    source.documents[1].content = content;
    EXPECT_FALSE(parser.refreshSource());
    EXPECT_FALSE(parser.isSourceValid());
  }
  source.documents[1].content = R"({"data":{"val":231}})";
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("0505:/data/val"), 231);
}
