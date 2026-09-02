// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mqtt_topic.h"

#include <string.h>
#include <stdio.h>

Supla::Protocol::MqttTopic::MqttTopic(const char *prefix) {
  append(prefix);
}

Supla::Protocol::MqttTopic::MqttTopic() {
}

void Supla::Protocol::MqttTopic::append(const char *suffix) {
  int currentLen = strlen(data);
  int suffixLen = strlen(suffix);

  if (currentLen + suffixLen < MAX_TOPIC_LEN) {
    memcpy(data + currentLen, suffix, suffixLen);
  }
}

Supla::Protocol::MqttTopic Supla::Protocol::MqttTopic::operator/(
    const char *suffix) {
  auto topic = *this;
  topic.append("/");
  topic.append(suffix);

  return topic;
}

Supla::Protocol::MqttTopic& Supla::Protocol::MqttTopic::operator/=(
  const char *suffix) {
  *this = (*this / suffix);
  return *this;
}

Supla::Protocol::MqttTopic Supla::Protocol::MqttTopic::operator/(
  const int suffix) {
  char buf[50] = {};
  snprintf(buf, sizeof(buf), "%d", suffix);
  auto topic = *this / buf;
  return topic;
}

Supla::Protocol::MqttTopic& Supla::Protocol::MqttTopic::operator/=(
  const int suffix) {
  *this = (*this / suffix);
  return *this;
}

Supla::Protocol::MqttTopic& Supla::Protocol::MqttTopic::operator+=(
    const char *suffix) {
  append(suffix);
  return *this;
}

const char *Supla::Protocol::MqttTopic::c_str() {
  return data;
}
