// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <linux_mqtt_client.h>
#include <mqtt_client.h>
#include <supla/source/mqtt_src.h>
#include <supla/output/mqtt.h>

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
