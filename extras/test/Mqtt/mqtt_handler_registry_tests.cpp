// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <gtest/gtest.h>
#include <supla/control/hvac_base.h>
#include <supla/protocol/mqtt.h>
#include <supla/protocol/mqtt/hvac_mqtt.h>
#include <supla/protocol/mqtt_handler_registry.h>

#include <array>

#include "../doubles/mqtt_mock.h"

namespace {

class InspectableMqtt : public MqttMock {
 public:
  explicit InspectableMqtt(SuplaDeviceClass *device) : MqttMock(device) {
  }

  Supla::Protocol::MqttChannelHandler *handlerFor(int channelType) const {
    return findChannelHandler(channelType);
  }
};

class TestHandler : public Supla::Protocol::MqttChannelHandler {
 public:
  TestHandler() : TestHandler(0) {
  }

  explicit TestHandler(int channelType) : channelType(channelType) {
  }

  void setChannelType(int newChannelType) {
    channelType = newChannelType;
  }

  int mqttHandledChannelType() const override {
    return channelType;
  }

  void mqttPublishChannelState(Supla::Protocol::Mqtt *,
                               Supla::Element *) override {
  }
  void mqttSubscribeChannel(Supla::Protocol::Mqtt *,
                            Supla::Element *) override {
  }
  bool mqttProcessData(Supla::Protocol::Mqtt *,
                       const char *,
                       const char *,
                       Supla::Element *) override {
    return false;
  }
  void mqttPublishHADiscovery(Supla::Protocol::Mqtt *,
                              Supla::Element *) override {
  }

 private:
  int channelType;
};

class MqttHandlerRegistryTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Protocol::MqttHandlerRegistry::instance().clearForTests();
  }

  void TearDown() override {
    Supla::Protocol::MqttHandlerRegistry::instance().clearForTests();
  }
};

TEST_F(MqttHandlerRegistryTests, HvacCreatedBeforeMqttIsVisibleToMqtt) {
  Supla::Control::HvacBase hvac;
  SuplaDeviceClass device;
  InspectableMqtt mqtt(&device);

  EXPECT_NE(nullptr, mqtt.handlerFor(SUPLA_CHANNELTYPE_HVAC));
}

TEST_F(MqttHandlerRegistryTests, HvacCreatedAfterMqttIsVisibleToMqtt) {
  SuplaDeviceClass device;
  InspectableMqtt mqtt(&device);
  EXPECT_EQ(nullptr, mqtt.handlerFor(SUPLA_CHANNELTYPE_HVAC));

  Supla::Control::HvacBase hvac;

  EXPECT_NE(nullptr, mqtt.handlerFor(SUPLA_CHANNELTYPE_HVAC));
}

TEST_F(MqttHandlerRegistryTests, TwoMqttInstancesUseSameGlobalHvacHandler) {
  Supla::Protocol::RegisterHvacMqttHandler();
  SuplaDeviceClass device;
  InspectableMqtt first(&device);
  InspectableMqtt second(&device);

  EXPECT_NE(nullptr, first.handlerFor(SUPLA_CHANNELTYPE_HVAC));
  EXPECT_EQ(first.handlerFor(SUPLA_CHANNELTYPE_HVAC),
            second.handlerFor(SUPLA_CHANNELTYPE_HVAC));
}

TEST_F(MqttHandlerRegistryTests, RegistrationIsIdempotentByPointerAndType) {
  TestHandler first(10001);
  TestHandler sameType(10001);
  auto &registry = Supla::Protocol::MqttHandlerRegistry::instance();

  EXPECT_TRUE(registry.registerHandler(&first));
  EXPECT_TRUE(registry.registerHandler(&first));
  EXPECT_TRUE(registry.registerHandler(&sameType));
  EXPECT_EQ(1u, registry.sizeForTests());
  EXPECT_EQ(&first, registry.findHandler(10001));
}

TEST_F(MqttHandlerRegistryTests, DoesNotFindUnsupportedChannelType) {
  EXPECT_EQ(nullptr,
            Supla::Protocol::MqttHandlerRegistry::instance().findHandler(
                SUPLA_CHANNELTYPE_ELECTRICITY_METER));
}

TEST_F(MqttHandlerRegistryTests, RejectsRegistrationBeyondCapacity) {
  std::array<TestHandler, SUPLA_MQTT_HANDLER_REGISTRY_MAX_HANDLERS + 1>
      handlers = {};
  auto &registry = Supla::Protocol::MqttHandlerRegistry::instance();

  for (size_t i = 0; i < handlers.size(); i++) {
    handlers[i].setChannelType(11000 + i);
  }
  for (size_t i = 0; i < SUPLA_MQTT_HANDLER_REGISTRY_MAX_HANDLERS; i++) {
    EXPECT_TRUE(registry.registerHandler(&handlers[i]));
  }
  EXPECT_FALSE(registry.registerHandler(
      &handlers[SUPLA_MQTT_HANDLER_REGISTRY_MAX_HANDLERS]));
  EXPECT_EQ(SUPLA_MQTT_HANDLER_REGISTRY_MAX_HANDLERS, registry.sizeForTests());
}

}  // namespace
