// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mqtt_mock.h"

#include <string>

MqttInterface::MqttInterface(SuplaDeviceClass *sdc)
    : Supla::Protocol::Mqtt(sdc),
      documentationRecorder_(&MqttDocumentationRecorder::fromEnvironment()) {
}

MqttInterface::~MqttInterface() {
}

void MqttInterface::publishImp(const char *topic,
                               const char *payload,
                               int qos,
                               bool retain) {
  documentationRecorder_->recordPublish(topic, payload, qos, retain);
  publishTest(std::string(topic), std::string(payload), qos, retain);
}

void MqttInterface::subscribeImp(const char *topic, int qos) {
  documentationRecorder_->recordSubscribe(topic, qos);
  subscribeTest(std::string(topic), qos);
}

MqttDocumentationRecorder &MqttInterface::documentationRecorder() {
  return *documentationRecorder_;
}

void MqttInterface::setDocumentationRecorder(
    MqttDocumentationRecorder *recorder) {
  documentationRecorder_ = recorder == nullptr
                               ? &MqttDocumentationRecorder::fromEnvironment()
                               : recorder;
}

MqttMock::MqttMock(SuplaDeviceClass *sdc) : MqttInterface(sdc) {
}

MqttMock::~MqttMock() {
}

void MqttMock::setRegisteredAndReady() {
  connected = true;
}
