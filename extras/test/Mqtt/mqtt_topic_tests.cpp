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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <supla/protocol/mqtt_topic.h>
#include <SuplaDevice.h>
#include <config_mock.h>
#include <network_with_mac_mock.h>

using testing::_;
using ::testing::SetArrayArgument;
using ::testing::DoAll;
using ::testing::Return;

TEST(MqttTopicTests, defaultCtrTests) {

  Supla::Protocol::MqttTopic topic;

  EXPECT_STREQ(topic.c_str(), "");

  topic = topic / "test";

  EXPECT_STREQ(topic.c_str(), "/test");
}


TEST(MqttTopicTests, charTests) {

  Supla::Protocol::MqttTopic topic("prefix");

  EXPECT_STREQ(topic.c_str(), "prefix");

  topic = topic / "test";

  EXPECT_STREQ(topic.c_str(), "prefix/test");

  topic /= "1";
  EXPECT_STREQ(topic.c_str(), "prefix/test/1");

  topic = topic / "2" / "3";
  EXPECT_STREQ(topic.c_str(), "prefix/test/1/2/3");
}

TEST(MqttTopicTests, intTests) {

  Supla::Protocol::MqttTopic topic("prefix");

  EXPECT_STREQ(topic.c_str(), "prefix");

  topic = topic / 3;

  EXPECT_STREQ(topic.c_str(), "prefix/3");

  topic /= 1;
  EXPECT_STREQ(topic.c_str(), "prefix/3/1");

  topic = topic / 2 / 3;
  EXPECT_STREQ(topic.c_str(), "prefix/3/1/2/3");
}


TEST(MqttTopicTests, copyTests) {

  Supla::Protocol::MqttTopic topic("prefix");

  EXPECT_STREQ(topic.c_str(), "prefix");

  auto topic2 = topic;

  topic = topic / "1";

  EXPECT_STREQ(topic.c_str(), "prefix/1");
  EXPECT_STREQ(topic2.c_str(), "prefix");


  topic2 /= "2";
  EXPECT_STREQ(topic2.c_str(), "prefix/2");

  topic = topic / "2" / "3";
  EXPECT_STREQ(topic.c_str(), "prefix/1/2/3");
  EXPECT_STREQ(topic2.c_str(), "prefix/2");

  EXPECT_STREQ((topic / "copy").c_str(), "prefix/1/2/3/copy");
  EXPECT_STREQ(topic.c_str(), "prefix/1/2/3");

}
