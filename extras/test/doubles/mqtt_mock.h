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

#ifndef EXTRAS_TEST_DOUBLES_MQTT_MOCK_H_
#define EXTRAS_TEST_DOUBLES_MQTT_MOCK_H_

#include <gmock/gmock.h>
#include <supla/protocol/mqtt.h>
#include <SuplaDevice.h>
#include <string>

class MqttInterface : public Supla::Protocol::Mqtt {
 public:
  explicit MqttInterface(SuplaDeviceClass *sdc);
  virtual ~MqttInterface();

  virtual void publishTest(std::string topic,
                           std::string paload,
                           int qos,
                           bool retain) = 0;
  void publishImp(const char *topic,
                  const char *payload,
                  int qos,
                  bool retain);
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
  MOCK_METHOD(void, subscribeImp, (const char *topic, int qos), (override));
  MOCK_METHOD(void, disconnect, (), (override));
  MOCK_METHOD(bool, iterate, (uint32_t _millis), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_MQTT_MOCK_H_
