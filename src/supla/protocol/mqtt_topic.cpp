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
