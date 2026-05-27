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
#include <supla/network/web_server.h>
#include <supla/storage/config_tags.h>

#include <cstdio>
#include <cstring>
#include <string>

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::StrEq;

class SenderMock : public Supla::WebSender {
 public:
  MOCK_METHOD(void, send, (const char*, int), (override));
};

class DummyWebServer : public Supla::WebServer {
 public:
  DummyWebServer() : WebServer(nullptr) {
  }
  void start() override {
  }
  void stop() override {
  }

  bool csrfValidatedState() const {
    return csrfValidated;
  }

  bool csrfRejectedState() const {
    return csrfRejected;
  }
};

class CountingHtmlElement : public Supla::HtmlElement {
 public:
  CountingHtmlElement() : Supla::HtmlElement(Supla::HTML_SECTION_FORM) {
  }

  void send(Supla::WebSender*) override {
  }

  bool handleResponse(const char* key, const char* value) override {
    lastKey = key ? key : "";
    lastValue = value ? value : "";
    handledCount++;
    return true;
  }

  std::string lastKey;
  std::string lastValue;
  int handledCount = 0;
};

class HtmlTagBuilderTests : public ::testing::Test {
 protected:
  void TearDown() override {
    EXPECT_EQ(Supla::HtmlElement::begin(), nullptr);
    EXPECT_EQ(Supla::Network::FirstInstance(), nullptr);
  }

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

TEST_F(HtmlTagBuilderTests, SelectInputSupportsOnChange) {
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  sender.selectTag("mode", "mode").attr("onchange", "update()").body([&]() {
    sender.selectOption(1, 1, true);
    sender.selectOption(2, "Two", false);
    sender.selectOption("3", 3, false);
  });

  EXPECT_EQ(sendHtml,
            "<select name=\"mode\" id=\"mode\" onchange=\"update()\">"
            "<option value=\"1\" selected>1</option>"
            "<option value=\"2\">Two</option>"
            "<option value=\"3\">3</option>"
            "</select>");
}

TEST_F(HtmlTagBuilderTests, LabelForAndSelectItemEscapeUnsafeText) {
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  sender.sendLabelFor("id\"<&", "lab\"<&");
  sender.sendSelectItem(7, "opt\"<&", true);

  EXPECT_EQ(sendHtml,
            "<label for=\"id&quot;&lt;&amp;\">lab&quot;&lt;&amp;</label>"
            "<option value=\"7\" selected>opt&quot;&lt;&amp;</option>");
}

TEST_F(HtmlTagBuilderTests, SendCsrfFieldRendersHiddenInput) {
  DummyWebServer server;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  const std::string token = server.getCsrfToken();
  ASSERT_EQ(token.size(), 32u);
  EXPECT_TRUE(server.isCsrfTokenValid(token.c_str()));
  EXPECT_FALSE(server.isCsrfTokenValid("0123456789abcdef0123456789abcdef"));

  sender.sendCsrfField();

  EXPECT_EQ(sendHtml,
            "<input type=\"hidden\" name=\"csrf\" value=\"" + token + "\">");
}

TEST_F(HtmlTagBuilderTests, ParsePostRequiresValidCsrfFirstField) {
  DummyWebServer server;
  CountingHtmlElement element;

  const std::string token = server.getCsrfToken();
  const std::string invalidBody = "foo=bar";
  server.parsePost(invalidBody.c_str(), invalidBody.size(), true);
  EXPECT_EQ(element.handledCount, 0);
  server.resetParser();

  const std::string validBody = "csrf=" + token + "&foo=bar";
  server.parsePost(validBody.c_str(), validBody.size(), true);
  EXPECT_EQ(element.handledCount, 1);
  EXPECT_EQ(element.lastKey, "foo");
  server.resetParser();
}

TEST_F(HtmlTagBuilderTests, ParsePostRejectsWhenCsrfIsNotFirstField) {
  DummyWebServer server;
  CountingHtmlElement element;

  const std::string validToken = server.getCsrfToken();
  const std::string body = "sid=beta&csrf=" + validToken;
  server.parsePost(body.c_str(), body.size(), true);
  EXPECT_FALSE(server.csrfValidatedState());
  EXPECT_TRUE(server.csrfRejectedState());
  EXPECT_EQ(element.handledCount, 0);
  server.resetParser();
}

TEST_F(HtmlTagBuilderTests, WifiParametersRegressionBeforeRefactor) {
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
  EXPECT_CALL(cfg, loadNetifConfig(StrEq(Supla::ConfigTag::WifiNetifCfgTag), _))
      .WillOnce(Return(false));
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

  EXPECT_THAT(sendHtml, HasSubstr("<h3>Wi-Fi Settings</h3>"));
  EXPECT_THAT(sendHtml, HasSubstr("for=\"wifi_en\">Enable Wi-Fi"));
  EXPECT_THAT(sendHtml,
              HasSubstr("name=\"sid\" id=\"sid\" value=\"ssid_test\""));
  EXPECT_THAT(sendHtml, HasSubstr("name=\"wpw\" id=\"wpw\""));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"wifi_mode\""));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"wifi_static_box\""));
  EXPECT_THAT(sendHtml, HasSubstr("showHideNetifStaticSettings"));
  EXPECT_THAT(sendHtml, HasSubstr("setAttribute(\"required\""));
  EXPECT_THAT(sendHtml, HasSubstr("removeAttribute(\"required\")"));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"wifi_ip\" maxlength=\"15\""));
  EXPECT_THAT(sendHtml, HasSubstr("inputmode=\"decimal\""));
  EXPECT_THAT(sendHtml, HasSubstr("placeholder=\"192.168.1.100\""));
  EXPECT_THAT(sendHtml, HasSubstr("pattern=\"^((25[0-5]"));
  EXPECT_THAT(sendHtml, HasSubstr("data-static-required=\"1\""));
  EXPECT_THAT(sendHtml, HasSubstr("for=\"wifi_mask\">Subnet mask</label>"));
  EXPECT_THAT(sendHtml, HasSubstr("placeholder=\"255.255.255.0 or /24\""));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"wifi_dns2\" maxlength=\"15\""));
  EXPECT_THAT(sendHtml, HasSubstr("placeholder=\"8.8.4.4\""));
}
