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

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_mock.h>
#include <supla/network/html/custom_text_parameter.h>
#include <supla/network/html/select_input_parameter.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/network/web_sender.h>

#include <cstring>
#include <cstdio>
#include <string>

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

class SenderMock : public Supla::WebSender {
 public:
  MOCK_METHOD(void, send, (const char*, int), (override));
};

class NetworkStateResetter : public Supla::Network {
 public:
  static void reset() {
    firstNetIntf = nullptr;
    netIntf = nullptr;
  }

  void setup() override {
  }
  void disable() override {
  }
  bool isReady() override {
    return false;
  }
};

class HtmlTagBuilderTests : public ::testing::Test {
 protected:
  void appendSentHtml(const char* data, int size) {
    if (size < 0) {
      sendHtml.append(data);
    } else {
      sendHtml.append(data, static_cast<size_t>(size));
    }
  }

  std::string sendHtml;
};

TEST_F(HtmlTagBuilderTests, BuildsNestedHtmlAndEscapes) {
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  {
    auto div = sender.tag("div");
    div.attr("class", "a\"b").body([&]() {
      sender.sendSafe("x<&");

      auto input = sender.voidTag("input");
      input.attr("type", "text").attr("value", "q<&").finish();
    });
  }

  EXPECT_EQ(sendHtml,
            "<div class=\"a&quot;b\">"
            "x&lt;&amp;"
            "<input type=\"text\" value=\"q&lt;&amp;\">"
            "</div>");
}

TEST_F(HtmlTagBuilderTests, CustomTextParameterUsesBuilder) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getStringSize(StrEq("test_tag"))).WillOnce(Return(5));
  EXPECT_CALL(cfg, getString(StrEq("test_tag"), _, 5))
      .WillOnce([](const char*, char* value, size_t) {
        std::memcpy(value, "A<B&C", sizeof("A<B&C"));
        return true;
      });
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::CustomTextParameter param("test_tag", "test_label", 20);
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"test_tag\">test_label</label>"
            "<input type=\"text\" maxlength=\"20\" name=\"test_tag\" "
            "id=\"test_tag\" value=\"A&lt;B&amp;C\">"
            "</div>");
}

TEST_F(HtmlTagBuilderTests, SelectInputParameterUsesBuilder) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getInt32(StrEq("test_tag"), _))
      .WillOnce([](const char*, int32_t* result) {
        *result = 2;
        return true;
      });
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::SelectInputParameter param("test_tag", "test_label");
  param.registerValue("one", 1);
  param.registerValue("two<&", 2);

  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"test_tag\">test_label</label>"
            "<select name=\"test_tag\" id=\"test_tag\">"
            "<option value=\"one\">one</option>"
            "<option value=\"two&lt;&amp;\" selected>two&lt;&amp;</option>"
            "</select>"
            "</div>");
}

TEST_F(HtmlTagBuilderTests, WifiParametersRegressionBeforeRefactor) {
  NetworkStateResetter::reset();

  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq(Supla::WifiDisableTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillOnce([](char* ssid) {
    std::memcpy(ssid, "ssid_test", sizeof("ssid_test"));
    return true;
  });
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  {
    NetworkMock net1;
    NetworkMock net2;
    (void)net1;
    (void)net2;

    Supla::Html::WifiParameters params;
    params.send(&sender);
  }

  NetworkStateResetter::reset();

  EXPECT_EQ(sendHtml,
            "<h3>Wi-Fi Settings</h3>"
            "<div class=\"form-field right-checkbox\">"
            "<label for=\"wifi_en\">Enable Wi-Fi</label>"
            "<label>"
            "<span class=\"switch\">"
            "<input type=\"checkbox\" value=\"on\" checked name=\"wifi_en\" "
            "id=\"wifi_en\">"
            "<span class=\"slider\"></span>"
            "</span>"
            "</label>"
            "</div>"
            "<div class=\"form-field\">"
            "<label for=\"sid\">Network name</label>"
            "<input type=\"text\" name=\"sid\" id=\"sid\" value=\"ssid_test\">"
            "</div>"
            "<div class=\"form-field\">"
            "<label for=\"wpw\">Password</label>"
            "<input type=\"password\" name=\"wpw\" id=\"wpw\">"
            "</div>");
}
