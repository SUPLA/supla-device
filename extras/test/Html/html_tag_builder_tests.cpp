// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_mock.h>
#include <simple_time.h>
#include <supla/device/last_state_logger.h>
#include <supla/network/html/custom_text_parameter.h>
#include <supla/network/html/select_input_parameter.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/network/netif_wifi.h>
#include <supla/network/web_sender.h>
#include <supla/network/web_server.h>
#include <supla/network/wifi_scan_result.h>
#include <supla/storage/config_tags.h>

#include <cstdio>
#include <cstring>
#include <string>

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Not;
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

class PeriodicScanWifi : public Supla::Wifi {
 public:
  void setup() override {
  }

  void disable() override {
  }

  bool isReady() override {
    return false;
  }

  void startConfigModeScan() override {
    scanStartCount++;
    scanInProgress = true;
  }

  void finishScan(uint32_t timestampMs) {
    auto cache = Supla::WifiScanResultCache::Instance();
    cache->beginUpdate();
    cache->addOrUpdate("ssid_test", -70, 1);
    cache->finishUpdate(timestampMs);
    scanInProgress = false;
  }

  int scanStartCount = 0;
  bool scanInProgress = false;

 protected:
  bool isConfigModeScanInProgress() const override {
    return scanInProgress;
  }
};

class HtmlTagBuilderTests : public ::testing::Test {
 protected:
  void TearDown() override {
    Supla::WifiScanResultCache::Instance()->clear();
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
  SimpleTime time;
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
  EXPECT_THAT(sendHtml,
              HasSubstr("<div><input type=\"text\" name=\"sid\" id=\"sid\""));
  EXPECT_THAT(sendHtml, HasSubstr("Wi-Fi scan result is not available yet"));
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

TEST_F(HtmlTagBuilderTests, WifiParametersShowsScanQualityForSavedSsid) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid_test", -74, 6);
  cache->finishUpdate(millis());

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

  EXPECT_THAT(sendHtml, HasSubstr("Found in Wi-Fi scan"));
  EXPECT_THAT(sendHtml, HasSubstr("RSSI -74 dBm"));
  EXPECT_THAT(sendHtml, HasSubstr("quality 52%"));
  EXPECT_THAT(sendHtml,
              HasSubstr("<div class=\"hint\" id=\"wifi_scan_hint\">"));
}

TEST_F(HtmlTagBuilderTests, WifiParametersHighlightsWeakScanQuality) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid_weak", -83, 6);
  cache->finishUpdate(millis());

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq(Supla::WifiDisableTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillOnce([](char* ssid) {
    std::memcpy(ssid, "ssid_weak", sizeof("ssid_weak"));
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

  EXPECT_THAT(sendHtml,
              HasSubstr("<div class=\"hint warn\" id=\"wifi_scan_hint\">"));
  EXPECT_THAT(sendHtml, HasSubstr("quality 34%"));
  EXPECT_THAT(sendHtml,
              HasSubstr("wifiScanHintClasses['ssid_weak']='hint warn'"));
}

TEST_F(HtmlTagBuilderTests, WifiParametersHighlightsVeryWeakScanQuality) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid_bad", -90, 6);
  cache->finishUpdate(millis());

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq(Supla::WifiDisableTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillOnce([](char* ssid) {
    std::memcpy(ssid, "ssid_bad", sizeof("ssid_bad"));
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

  EXPECT_THAT(sendHtml,
              HasSubstr("<div class=\"hint error\" id=\"wifi_scan_hint\">"));
  EXPECT_THAT(sendHtml, HasSubstr("quality 20%"));
  EXPECT_THAT(sendHtml,
              HasSubstr("wifiScanHintClasses['ssid_bad']='hint error'"));
}

TEST_F(HtmlTagBuilderTests, WifiParametersHighlightsNotFoundNetworkAsWarning) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("other_ssid", -50, 6);
  cache->finishUpdate(millis());

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq(Supla::WifiDisableTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillOnce([](char* ssid) {
    std::memcpy(ssid, "hidden_ssid", sizeof("hidden_ssid"));
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

  EXPECT_THAT(sendHtml,
              HasSubstr("<div class=\"hint warn\" id=\"wifi_scan_hint\">"));
  EXPECT_THAT(sendHtml, HasSubstr("Warning: this network was not found"));
  EXPECT_THAT(sendHtml, HasSubstr("h.className='hint warn';"));
}

TEST_F(HtmlTagBuilderTests, WifiParametersKeepsSsidInputAndAddsDatalist) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid_test", -74, 6);
  cache->addOrUpdate("ssid_other", -45, 11);
  cache->finishUpdate(millis());

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

  EXPECT_THAT(sendHtml, HasSubstr("type=\"text\""));
  EXPECT_THAT(sendHtml, HasSubstr("name=\"sid\""));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"sid\""));
  EXPECT_THAT(sendHtml, HasSubstr("list=\"wifi_ssid_list\""));
  EXPECT_THAT(sendHtml, HasSubstr("oninput=\"wifiSsidChanged()\""));
  EXPECT_THAT(sendHtml, HasSubstr("onchange=\"wifiSsidChanged()\""));
  EXPECT_THAT(sendHtml,
              HasSubstr("<div class=\"hint\" id=\"wifi_scan_hint\">"));
  EXPECT_THAT(sendHtml, HasSubstr("<datalist id=\"wifi_ssid_list\">"));
  EXPECT_THAT(
      sendHtml,
      HasSubstr("<option value=\"ssid_test\">ssid_test</option>"));
  EXPECT_THAT(
      sendHtml,
      HasSubstr("<option value=\"ssid_other\">ssid_other</option>"));
  EXPECT_THAT(sendHtml,
              Not(HasSubstr("RSSI -74 dBm, quality 52%, ch 6")));
  EXPECT_THAT(sendHtml,
              HasSubstr("Found in Wi-Fi scan: RSSI -74 dBm, quality 52%"));
  EXPECT_THAT(sendHtml, HasSubstr("var wifiScanHints=Object.create(null);"));
  EXPECT_THAT(sendHtml,
              HasSubstr("var wifiScanHintClasses=Object.create(null);"));
  EXPECT_THAT(sendHtml, HasSubstr("function wifiSsidChanged()"));
  EXPECT_THAT(sendHtml,
              HasSubstr("h.className=wifiScanHintClasses[e.value]||'hint';"));
}

TEST_F(HtmlTagBuilderTests, WifiParametersEscapesSsidForDatalistScript) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid_'\\</script>&", -60, 1);
  cache->finishUpdate(millis());

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq(Supla::WifiDisableTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillOnce([](char* ssid) {
    std::memcpy(ssid, "ssid_'\\</script>&", sizeof("ssid_'\\</script>&"));
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

  EXPECT_THAT(
      sendHtml,
      HasSubstr("<option value=\"ssid_&apos;\\&lt;/script&gt;&amp;\">"
                "ssid_&apos;\\&lt;/script&gt;&amp;</option>"));
  EXPECT_THAT(sendHtml, HasSubstr("wifiScanHints['ssid_\\'\\\\"));
  EXPECT_THAT(sendHtml, HasSubstr("\\x3C/script\\x3E\\x26']"));
}

TEST_F(HtmlTagBuilderTests, WifiParametersLogsScanResultToLastStateOnWifiSave) {
  ConfigMock cfg;
  DummyWebServer server;
  SuplaDeviceClass sdc;
  auto logger = new Supla::Device::LastStateLogger;
  sdc.setLastStateLogger(logger);
  server.setSuplaDeviceClass(&sdc);

  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid_test", -74, 6);
  cache->finishUpdate(millis());

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setWiFiSSID(StrEq("ssid_test"))).WillOnce(Return(true));
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillOnce([](char* ssid) {
    std::memcpy(ssid, "ssid_test", sizeof("ssid_test"));
    return true;
  });

  {
    Supla::Html::WifiParameters params;
    EXPECT_TRUE(params.handleResponse("sid", "ssid_test"));
    params.onProcessingEnd();
  }

  ASSERT_TRUE(logger->prepareLastStateLog());
  char* log = logger->getLog();
  ASSERT_NE(nullptr, log);
  EXPECT_THAT(log, HasSubstr("Wi-Fi: \"ssid_test\" found"));
  EXPECT_THAT(log, HasSubstr("RSSI -74 dBm"));
  EXPECT_THAT(log, HasSubstr("quality 52%"));
  EXPECT_EQ(nullptr, logger->getLog());
}

TEST_F(HtmlTagBuilderTests,
       WifiParametersDoesNotLogScanResultOnRestartWithoutWifiSave) {
  ConfigMock cfg;
  DummyWebServer server;
  SuplaDeviceClass sdc;
  auto logger = new Supla::Device::LastStateLogger;
  sdc.setLastStateLogger(logger);
  server.setSuplaDeviceClass(&sdc);

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, loadNetifConfig(StrEq(Supla::ConfigTag::WifiNetifCfgTag), _))
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getWiFiSSID(_)).Times(0);

  {
    Supla::Html::WifiParameters params;
    EXPECT_FALSE(params.handleResponse("rbt", "2"));
    params.onProcessingEnd();
  }

  ASSERT_TRUE(logger->prepareLastStateLog());
  EXPECT_EQ(nullptr, logger->getLog());
}

TEST_F(HtmlTagBuilderTests, WifiParametersDeduplicatesSameScanLastState) {
  ConfigMock cfg;
  DummyWebServer server;
  SuplaDeviceClass sdc;
  auto logger = new Supla::Device::LastStateLogger;
  sdc.setLastStateLogger(logger);
  server.setSuplaDeviceClass(&sdc);

  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid_dedup", -74, 6);
  cache->finishUpdate(millis());

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setWiFiSSID(StrEq("ssid_dedup")))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char* ssid) {
    std::memcpy(ssid, "ssid_dedup", sizeof("ssid_dedup"));
    return true;
  });

  {
    Supla::Html::WifiParameters params;
    EXPECT_TRUE(params.handleResponse("sid", "ssid_dedup"));
    params.onProcessingEnd();
    EXPECT_TRUE(params.handleResponse("sid", "ssid_dedup"));
    params.onProcessingEnd();
  }

  ASSERT_TRUE(logger->prepareLastStateLog());
  char* log = logger->getLog();
  ASSERT_NE(nullptr, log);
  EXPECT_THAT(log, HasSubstr("Wi-Fi: \"ssid_dedup\" found"));
  EXPECT_EQ(nullptr, logger->getLog());
}

TEST_F(HtmlTagBuilderTests, WifiParametersLogsScanLastStateWhenMessageChanges) {
  ConfigMock cfg;
  DummyWebServer server;
  SuplaDeviceClass sdc;
  auto logger = new Supla::Device::LastStateLogger;
  sdc.setLastStateLogger(logger);
  server.setSuplaDeviceClass(&sdc);

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setWiFiSSID(StrEq("ssid_changed")))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char* ssid) {
    std::memcpy(ssid, "ssid_changed", sizeof("ssid_changed"));
    return true;
  });

  {
    Supla::Html::WifiParameters params;
    auto cache = Supla::WifiScanResultCache::Instance();
    cache->beginUpdate();
    cache->addOrUpdate("ssid_changed", -74, 6);
    cache->finishUpdate(millis());
    EXPECT_TRUE(params.handleResponse("sid", "ssid_changed"));
    params.onProcessingEnd();

    cache->beginUpdate();
    cache->addOrUpdate("ssid_changed", -50, 6);
    cache->finishUpdate(millis());
    EXPECT_TRUE(params.handleResponse("sid", "ssid_changed"));
    params.onProcessingEnd();
  }

  ASSERT_TRUE(logger->prepareLastStateLog());
  char* log = logger->getLog();
  ASSERT_NE(nullptr, log);
  EXPECT_THAT(log, HasSubstr("RSSI -50 dBm"));
  log = logger->getLog();
  ASSERT_NE(nullptr, log);
  EXPECT_THAT(log, HasSubstr("RSSI -74 dBm"));
  EXPECT_EQ(nullptr, logger->getLog());
}

TEST_F(HtmlTagBuilderTests, PeriodicWifiScanStartsWhenCacheIsMissing) {
  Supla::WifiScanResultCache::Instance()->clear();
  {
    PeriodicScanWifi wifi;
    Supla::Network::SetConfigMode();

    EXPECT_FALSE(Supla::Network::Iterate());
    EXPECT_EQ(1, wifi.scanStartCount);

    EXPECT_FALSE(Supla::Network::Iterate());
    EXPECT_EQ(1, wifi.scanStartCount);

    Supla::Network::SetNormalMode();
  }
}

TEST_F(HtmlTagBuilderTests, PeriodicWifiScanRefreshesAfterInterval) {
  Supla::WifiScanResultCache::Instance()->clear();
  {
    PeriodicScanWifi wifi;
    Supla::Network::SetConfigMode();
    wifi.finishScan(millis());

    EXPECT_FALSE(Supla::Network::Iterate());
    EXPECT_EQ(0, wifi.scanStartCount);

    time.advance(Supla::WifiScanRefreshIntervalMs - 1);
    EXPECT_FALSE(Supla::Network::Iterate());
    EXPECT_EQ(0, wifi.scanStartCount);

    time.advance(1);
    EXPECT_FALSE(Supla::Network::Iterate());
    EXPECT_EQ(1, wifi.scanStartCount);

    Supla::Network::SetNormalMode();
  }
}
