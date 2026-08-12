// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_MQTT_MOCK_H_
#define EXTRAS_TEST_DOUBLES_MQTT_MOCK_H_

#include <SuplaDevice.h>
#include <gmock/gmock.h>
#include <supla/protocol/mqtt.h>

#include <string>

#include "mqtt_documentation.h"

class MqttInterface : public Supla::Protocol::Mqtt {
 public:
  explicit MqttInterface(SuplaDeviceClass *sdc);
  virtual ~MqttInterface();

  virtual void publishTest(std::string topic,
                           std::string paload,
                           int qos,
                           bool retain) = 0;
  void publishImp(const char *topic, const char *payload, int qos, bool retain);
  virtual void subscribeTest(std::string topic, int qos) = 0;
  void subscribeImp(const char *topic, int qos) override;
  MqttDocumentationRecorder &documentationRecorder();

  void setDocumentationRecorder(MqttDocumentationRecorder *recorder);

 private:
  MqttDocumentationRecorder *documentationRecorder_;
};

class MqttMock : public MqttInterface {
 public:
  explicit MqttMock(SuplaDeviceClass *sdc);
  virtual ~MqttMock();

  void setRegisteredAndReady();

  MOCK_METHOD(void,
              publishTest,
              (std::string topic, std::string payload, int qos, bool retain),
              (override));
  MOCK_METHOD(void, subscribeTest, (std::string topic, int qos), (override));
  MOCK_METHOD(void, disconnect, (), (override));
  MOCK_METHOD(bool, iterate, (uint32_t _millis), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_MQTT_MOCK_H_
