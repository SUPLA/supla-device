/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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
