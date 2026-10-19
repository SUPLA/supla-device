// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <linux_mqtt_client.h>
#include <mqtt_client.h>
#include <supla/source/mqtt_src.h>
#include <supla/output/mqtt.h>

#include <simple_time.h>
#include <supla/parser/simple.h>
#include <supla/element.h>
#include <supla/sensor/electricity_meter_parsed.h>

#include <atomic>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace {
std::unordered_map<std::string, std::string> initialTopics;
bool modifyTopicsDuringInit = false;
int transportStarts = 0;

class MqttTestConfig : public Supla::LinuxYamlConfig {
 public:
  MqttTestConfig() : LinuxYamlConfig("") {
    config = YAML::Load("mqtt: {host: localhost, use_ssl: false}");
  }
  using LinuxYamlConfig::config;
};

void receive(const std::string& topic, const std::string& message) {
  mqtt_response_publish published = {};
  published.topic_name = topic.data();
  published.topic_name_size = topic.size();
  published.application_message = message.data();
  published.application_message_size = message.size();
  Supla::LinuxMqttClient::publishCallback(nullptr, &published);
}
}  // namespace

// Replace transport startup only; exercise the real client and source without
// a broker or a background network thread.
int mqtt_client_init(std::string, int, std::string, std::string, std::string,
                     const std::unordered_map<std::string, std::string>& topics,
                     void (*)(void**, struct mqtt_response_publish*)) {
  ++transportStarts;
  if (modifyTopicsDuringInit) {
    Supla::LinuxMqttClient::getInstance()->subscribeTopic("mqtt-test/init-new");
    receive("mqtt-test/init", "updated");
  }
  initialTopics = topics;
  return 0;
}

void mqtt_client_free() {
}

class LinuxMqttTests : public ::testing::Test {
 protected:
  MqttTestConfig config;
  std::shared_ptr<Supla::LinuxMqttClient> client =
      Supla::LinuxMqttClient::getInstance(config);
  void TearDown() override {
    client.reset();
    Supla::LinuxMqttClient::getInstance().reset();
  }
};

TEST_F(LinuxMqttTests, InvalidSecurityCannotCreateSourcesOutputsOrTransport) {
  client.reset();
  Supla::LinuxMqttClient::getInstance().reset();
  const int before = transportStarts;
  for (const char* fields : {"", "use_ssl: true", "use_ssl: broken",
                             "use_ssl: true, verify_ca: []",
                             "use_ssl: false, port: 8883"}) {
    SCOPED_TRACE(fields);
    config.config = YAML::Load(std::string("mqtt: {host: localhost, ") +
                               fields + "}");
    EXPECT_THROW(Supla::LinuxMqttClient::getInstance(config),
                 std::invalid_argument);
    EXPECT_THROW((Supla::Source::Mqtt(config, {"mqtt-test/tls"})),
                 std::invalid_argument);
    EXPECT_THROW((Supla::Output::Mqtt(config, "mqtt-test/tls", 0)),
                 std::invalid_argument);
    EXPECT_FALSE(Supla::LinuxMqttClient::getInstance());
    Supla::LinuxMqttClient::start();
    EXPECT_EQ(transportStarts, before);
  }
}

TEST_F(LinuxMqttTests, ExplicitTlsChoicesAndCaFileReachTheClientUnchanged) {
  for (const char* fields : {"use_ssl: false",
                             "use_ssl: true, verify_ca: false",
                             "use_ssl: true, verify_ca: true",
                             "use_ssl: true, verify_ca: true, "
                             "ca_file: ca.pem"}) {
    SCOPED_TRACE(fields);
    client.reset();
    Supla::LinuxMqttClient::getInstance().reset();
    config.config = YAML::Load(std::string("mqtt: {host: localhost, ") +
                               fields + "}");
    client = Supla::LinuxMqttClient::getInstance(config);
    ASSERT_TRUE(client);
    EXPECT_EQ(client->useSSL, config.getMqttClientUseSSL());
    EXPECT_EQ(client->verifyCA, config.getMqttClientVerifyCA());
    EXPECT_EQ(client->fileCA,
              config.config["mqtt"]["ca_file"].as<std::string>(""));
    const int before = transportStarts;
    Supla::LinuxMqttClient::start();
    EXPECT_EQ(transportStarts, before + 1);
  }
}

TEST_F(LinuxMqttTests, ReturnsIndependentCopyOfLatestMessage) {
  const std::string topic = "mqtt-test/copy";
  client->subscribeTopic(topic);
  std::string message;
  EXPECT_TRUE(client->getMessage(topic, message));
  EXPECT_TRUE(message.empty());
  receive(topic, "first");
  EXPECT_TRUE(client->getMessage(topic, message));
  receive(topic, "last");
  EXPECT_EQ(message, "first");
  EXPECT_TRUE(client->getMessage(topic, message));
  EXPECT_EQ(message, "last");
  client->unsubscribeTopic(topic);
  EXPECT_FALSE(client->getMessage(topic, message));
}

TEST_F(LinuxMqttTests, CombinesTopicsInConfiguredOrderIncludingEmptyMessages) {
  Supla::Source::Mqtt source(config,
      {"mqtt-test/first", "mqtt-test/empty", "mqtt-test/last"});
  EXPECT_TRUE(source.getContent().empty());
  receive("mqtt-test/last", "last");
  receive("mqtt-test/first", "first");
  EXPECT_EQ(source.getContent(), "first\n\nlast");
  client->unsubscribeTopic("mqtt-test/empty");
  EXPECT_EQ(source.getContent(), "first\nlast");
  receive("mqtt-test/first", "updated");
  EXPECT_EQ(source.getContent(), "updated\nlast");
  client->unsubscribeTopic("mqtt-test/first");
  EXPECT_EQ(source.getContent(), "last");
}

TEST_F(LinuxMqttTests, RetainsExistingExactLookupForWildcardSubscription) {
  Supla::Source::Mqtt source(config, {"mqtt-test/wildcard/#"});
  receive("mqtt-test/wildcard/child", "child");
  EXPECT_TRUE(source.getContent().empty());
  std::string message;
  EXPECT_TRUE(client->getMessage("mqtt-test/wildcard/child", message));
  EXPECT_EQ(message, "child");
  client->unsubscribeTopic("mqtt-test/wildcard/child");
}

TEST_F(LinuxMqttTests, InitializationUsesSnapshotOutsideMapLock) {
  client->subscribeTopic("mqtt-test/init");
  receive("mqtt-test/init", "before");
  modifyTopicsDuringInit = true;
  Supla::LinuxMqttClient::start();
  modifyTopicsDuringInit = false;
  EXPECT_EQ(initialTopics.at("mqtt-test/init"), "before");
  EXPECT_EQ(initialTopics.count("mqtt-test/init-new"), 0);
  std::string message;
  EXPECT_TRUE(client->getMessage("mqtt-test/init", message));
  EXPECT_EQ(message, "updated");
  client->unsubscribeTopic("mqtt-test/init");
  client->unsubscribeTopic("mqtt-test/init-new");
}

TEST_F(LinuxMqttTests, ConcurrentReceiveAndContentReadPreserveWholeMessages) {
  Supla::Source::Mqtt source(config,
                            {"mqtt-test/concurrent", "mqtt-test/tail"});
  const std::string first(512, 'a');
  const std::string second(1024, 'b');
  receive("mqtt-test/concurrent", first);
  receive("mqtt-test/tail", "tail");
  std::atomic<bool> start(false);
  std::thread writer([&]() {
    while (!start.load()) {
      std::this_thread::yield();
    }
    for (int i = 0; i < 10000; i++) {
      receive("mqtt-test/concurrent", i % 2 == 0 ? first : second);
    }
  });
  start = true;
  for (int i = 0; i < 10000; i++) {
    const auto content = source.getContent();
    EXPECT_TRUE(content == first + "\ntail" || content == second + "\ntail");
  }
  writer.join();
  EXPECT_EQ(source.getContent(), second + "\ntail");
}

TEST_F(LinuxMqttTests, ConcurrentSubscribeEraseReceiveAndRead) {
  const std::string topic = "mqtt-test/churn";
  client->subscribeTopic(topic);
  const std::string payload(512, 'x');
  std::atomic<bool> start(false);
  std::thread writer([&]() {
    while (!start.load()) {
      std::this_thread::yield();
    }
    for (int i = 0; i < 10000; i++) {
      receive(topic, payload);
    }
  });
  std::thread subscriptions([&]() {
    while (!start.load()) {
      std::this_thread::yield();
    }
    for (int i = 0; i < 10000; i++) {
      client->unsubscribeTopic(topic);
      client->subscribeTopic(topic);
      const auto other = "mqtt-test/rehash/" + std::to_string(i);
      client->subscribeTopic(other);
    }
    for (int i = 0; i < 10000; i++) {
      client->unsubscribeTopic("mqtt-test/rehash/" + std::to_string(i));
    }
  });
  start = true;
  for (int i = 0; i < 10000; i++) {
    std::string message;
    if (client->getMessage(topic, message)) {
      EXPECT_TRUE(message.empty() || message == payload);
    }
  }
  writer.join();
  subscriptions.join();
  receive(topic, "final");
  std::string message;
  EXPECT_TRUE(client->getMessage(topic, message));
  EXPECT_EQ(message, "final");
  client->unsubscribeTopic(topic);
  EXPECT_FALSE(client->getMessage(topic, message));
}

TEST_F(LinuxMqttTests, DocumentSnapshotsPreserveIdentityContentAndReceipt) {
  Supla::Source::Mqtt source(config,
      {"docs/0508", "docs/0505"}, {"0508", "0505"});
  auto documents = source.getDocuments();
  ASSERT_EQ(documents.size(), 2u);
  EXPECT_EQ(documents[0].id, "0508");
  EXPECT_FALSE(documents[0].received);
  receive("docs/0505", "{\n\"data\": {\"val\": 230.5}\n}");
  receive("docs/0508", "");
  documents = source.getDocuments();
  EXPECT_TRUE(documents[0].received);
  EXPECT_TRUE(documents[0].content.empty());
  EXPECT_TRUE(documents[1].received);
  EXPECT_EQ(documents[1].content, "{\n\"data\": {\"val\": 230.5}\n}");
  receive("docs/0508", "4.2");
  EXPECT_EQ(source.getDocuments()[0].content, "4.2");
  EXPECT_TRUE(documents[0].content.empty());
  EXPECT_EQ(source.getDocuments()[1].content, documents[1].content);
  EXPECT_EQ(source.getContent(), "4.2\n" + documents[1].content);
}

TEST_F(LinuxMqttTests, DocumentsExpireIndependentlyAndRecover) {
  Supla::Source::Mqtt source(config,
      {"expiry/a", "expiry/b"}, {"a", "b"}, 1);
  Supla::Source::Mqtt unlimited(config,
      {"expiry/a", "expiry/b"}, {"a", "b"});
  receive("expiry/a", "old");
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  receive("expiry/b", "new");
  auto documents = source.getDocuments();
  EXPECT_TRUE(documents[0].received);
  EXPECT_FALSE(documents[0].fresh);
  EXPECT_EQ(documents[0].content, "old");
  EXPECT_TRUE(documents[1].fresh);
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_TRUE(unlimited.getDocuments()[0].fresh);
  EXPECT_EQ(unlimited.getContent(), "old\nnew");
  receive("expiry/a", "renewed");
  EXPECT_TRUE(source.getDocuments()[0].fresh);
  EXPECT_EQ(source.getDocuments()[0].content, "renewed");
}

TEST_F(LinuxMqttTests, ConcurrentDocumentSnapshotKeepsWholePayloadAndMetadata) {
  Supla::Source::Mqtt source(config, {"snapshot/a", "snapshot/b"}, {"a", "b"});
  const std::string first(512, 'a'), second(1024, 'b');
  receive("snapshot/a", first);
  receive("snapshot/b", "constant");
  std::thread writer([&]() {
    for (int i = 0; i < 10000; ++i) {
      receive("snapshot/a", i % 2 ? first : second);
    }
  });
  for (int i = 0; i < 10000; ++i) {
    const auto documents = source.getDocuments();
    EXPECT_EQ(documents[0].id, "a");
    EXPECT_TRUE(documents[0].received);
    EXPECT_TRUE(documents[0].fresh);
    EXPECT_TRUE(documents[0].content == first ||
                documents[0].content == second);
    EXPECT_EQ(documents[1].content, "constant");
  }
  writer.join();
}

TEST_F(LinuxMqttTests, SimpleParserKeepsLegacyIndicesWithMissingMessages) {
  Supla::Source::Mqtt source(config, {"simple/a", "simple/b"});
  Supla::Parser::Simple parser(&source);
  parser.addKey("temperature", 0);
  parser.addKey("humidity", 1);
  EXPECT_FALSE(parser.refreshSource());
  receive("simple/b", "48");
  ASSERT_TRUE(parser.refreshSource());
  // Preserve the legacy omission of leading empty entries in getContent().
  EXPECT_DOUBLE_EQ(parser.getValue("temperature"), 48);
  EXPECT_EQ(parser.getValue("humidity"), 0);
  EXPECT_FALSE(parser.isValid());
  receive("simple/b", "");
  receive("simple/a", "21.5");
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("temperature"), 21.5);
  EXPECT_EQ(parser.getValue("humidity"), 0);
  EXPECT_FALSE(parser.isValid());
  receive("simple/b", "48");
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("humidity"), 48);
  EXPECT_DOUBLE_EQ(parser.getValue("temperature"), 21.5);
}

TEST_F(LinuxMqttTests,
       YamlMeterReadsIndependentDocumentsAndRejectsIncompleteData) {
  SimpleTime time;
  mqtt_client transport = {};
  transport.error = MQTT_OK;
  client->mq_client = &transport;
  config.config = YAML::Load(R"(
mqtt: {host: localhost, use_ssl: false}
channels:
- type: ElectricityMeterParsed
  name: meter
  source:
    type: MQTT
    state_topic: data/dev/1/0b04
    sub_topics: ["0509", "0508", "0505"]
    expiration_time_sec: 600
  parser: {type: Json, multi_document: true, refresh_time_ms: 1000}
  phase_1:
  - voltage: "0505:/data/val"
  - current: "0508:/data/val"
  - power_active: "0509:/data/val"
)");
  ASSERT_EQ(Supla::Element::begin(), nullptr);
  ASSERT_TRUE(config.loadChannels());
  auto* meter = dynamic_cast<Supla::Sensor::ElectricityMeterParsed*>(
      Supla::Element::begin());
  ASSERT_NE(meter, nullptr);
  std::unique_ptr<Supla::Element> cleanup(meter);
  receive("data/dev/1/0b04/0505", R"({"data":{"val":"230.5"}})");
  receive("data/dev/1/0b04/0508", R"({"data":{"val":"4.2"}})");
  meter->onInit();
  EXPECT_EQ(meter->getVoltage(0), 0);
  EXPECT_EQ(meter->getCurrent(0), 0u);
  TElectricityMeter_ExtendedValue_V3 value = {};
  meter->updateChannelValues();
  value = *reinterpret_cast<TElectricityMeter_ExtendedValue_V3*>(
      meter->getChannel()->getExtValue()->value);
  EXPECT_EQ(value.m_count, 0);
  // Cache hits must not turn an incomplete snapshot into a valid measurement.
  meter->readValuesFromDevice();
  meter->updateChannelValues();
  value = *reinterpret_cast<TElectricityMeter_ExtendedValue_V3*>(
      meter->getChannel()->getExtValue()->value);
  EXPECT_EQ(value.m_count, 0);
  receive("data/dev/1/0b04/0509", R"({"data":{"val":"966.0"}})");
  time.advance(1001);
  meter->readValuesFromDevice();
  EXPECT_EQ(meter->getVoltage(0), 23050);
  EXPECT_EQ(meter->getCurrent(0), 4200u);
  EXPECT_EQ(meter->getPowerActive(0), 96600000);
  meter->updateChannelValues();
  value = *reinterpret_cast<TElectricityMeter_ExtendedValue_V3*>(
      meter->getChannel()->getExtValue()->value);
  EXPECT_EQ(value.m_count, 1);
  EXPECT_NE(value.measured_values & EM_VAR_VOLTAGE, 0u);
  EXPECT_NE(value.measured_values & EM_VAR_CURRENT, 0u);
  EXPECT_NE(value.measured_values & EM_VAR_POWER_ACTIVE, 0u);
  // A conversion failure must not publish a valid zero, including cache hits.
  receive("data/dev/1/0b04/0508", R"({"data":{"val":"broken"}})");
  time.advance(1001);
  meter->readValuesFromDevice();
  meter->updateChannelValues();
  value = *reinterpret_cast<TElectricityMeter_ExtendedValue_V3*>(
      meter->getChannel()->getExtValue()->value);
  EXPECT_EQ(value.m_count, 0);
  meter->readValuesFromDevice();
  meter->updateChannelValues();
  value = *reinterpret_cast<TElectricityMeter_ExtendedValue_V3*>(
      meter->getChannel()->getExtValue()->value);
  EXPECT_EQ(value.m_count, 0);
  receive("data/dev/1/0b04/0508", R"({"data":{"val":5}})");
  time.advance(1001);
  meter->readValuesFromDevice();
  EXPECT_EQ(meter->getCurrent(0), 5000u);
  EXPECT_EQ(meter->getVoltage(0), 23050);
  client->mq_client = nullptr;
}

TEST_F(LinuxMqttTests, ExpirationNeverShiftsSimpleParserIndices) {
  Supla::Source::Mqtt source(config,
      {"expiry-index/a", "expiry-index/b", "expiry-index/c"},
      {"a", "b", "c"}, 1);
  Supla::Parser::Simple parser(&source);
  parser.addKey("a", 0);
  parser.addKey("b", 1);
  parser.addKey("c", 2);

  receive("expiry-index/a", "10");
  receive("expiry-index/c", "30");
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_FALSE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("b"), 0);
  EXPECT_FALSE(parser.isValid());

  receive("expiry-index/b", "20");
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("b"), 20);
  EXPECT_TRUE(parser.isValid());

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  receive("expiry-index/a", "11");
  receive("expiry-index/c", "31");
  const auto documents = source.getDocuments();
  EXPECT_FALSE(documents[1].fresh);
  EXPECT_EQ(documents[1].content, "20");
  EXPECT_TRUE(documents[0].fresh);
  EXPECT_TRUE(documents[2].fresh);
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_FALSE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("b"), 0);
  EXPECT_FALSE(parser.isValid());

  receive("expiry-index/b", "21");
  EXPECT_EQ(source.getContent(), "11\n21\n31");
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("a"), 11);
  EXPECT_DOUBLE_EQ(parser.getValue("b"), 21);
  EXPECT_DOUBLE_EQ(parser.getValue("c"), 31);
  EXPECT_TRUE(parser.isValid());

  // A received empty payload also keeps its own position.
  receive("expiry-index/a", "");
  EXPECT_EQ(source.getContent(), "\n21\n31");
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("b"), 21);
  EXPECT_TRUE(parser.isValid());
  EXPECT_DOUBLE_EQ(parser.getValue("a"), 0);
  EXPECT_FALSE(parser.isValid());
}
