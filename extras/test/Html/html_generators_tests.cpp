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

#include <SuplaDevice.h>
#include <clock_mock.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <output_mock.h>
#include <simple_time.h>
#include <supla-common/proto.h>
#include <supla/control/hvac_base.h>
#include <supla/control/relay.h>
#include <supla/control/rgb_cct_base.h>
#include <supla/control/roller_shutter.h>
#include <supla/device/register_device.h>
#include <supla/device/security_logger.h>
#include <supla/device/sw_update.h>
#include <supla/network/html/binary_sensor_parameters.h>
#include <supla/network/html/button_action_trigger_config.h>
#include <supla/network/html/button_config_parameters.h>
#include <supla/network/html/button_hold_time_parameters.h>
#include <supla/network/html/button_multiclick_parameters.h>
#include <supla/network/html/button_refresh.h>
#include <supla/network/html/button_type_parameters.h>
#include <supla/network/html/channel_correction.h>
#include <supla/network/html/container_parameters.h>
#include <supla/network/html/custom_checkbox_parameter.h>
#include <supla/network/html/custom_parameter.h>
#include <supla/network/html/custom_sw_update.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/disable_user_interface_parameter.h>
#include <supla/network/html/div.h>
#include <supla/network/html/em_ct_type.h>
#include <supla/network/html/em_phase_led.h>
#include <supla/network/html/ethernet_parameters.h>
#include <supla/network/html/h2_tag.h>
#include <supla/network/html/h3_tag.h>
#include <supla/network/html/hide_show_container.h>
#include <supla/network/html/home_screen_content.h>
#include <supla/network/html/hvac_parameters.h>
#include <supla/network/html/modbus_parameters.h>
#include <supla/network/html/power_status_led_parameters.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/pwm_frequency_parameters.h>
#include <supla/network/html/relay_parameters.h>
#include <supla/network/html/rgbw_button_parameters.h>
#include <supla/network/html/roller_shutter_parameters.h>
#include <supla/network/html/screen_brightness_parameters.h>
#include <supla/network/html/screen_delay_parameters.h>
#include <supla/network/html/screen_delay_type_parameters.h>
#include <supla/network/html/security_log_list.h>
#include <supla/network/html/select_cmd_input_parameter.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/sw_update.h>
#include <supla/network/html/sw_update_beta.h>
#include <supla/network/html/text_cmd_input_parameter.h>
#include <supla/network/html/time_parameters.h>
#include <supla/network/html/volume_parameters.h>
#include <supla/network/network.h>
#include <supla/network/web_sender.h>
#include <supla/sensor/binary_base.h>
#include <supla/sensor/container.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/storage/config_tags.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using ::testing::_;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StartsWith;
using ::testing::StrEq;

class SenderMock : public Supla::WebSender {
 public:
  MOCK_METHOD(void, send, (const char*, int), (override));
};

class HtmlCaptureTest : public ::testing::Test {
 protected:
  void appendSentHtml(const char* data, int size) {
    if (size < 0) {
      sendHtml.append(data);
    } else {
      sendHtml.append(data, static_cast<size_t>(size));
    }
  }

  void expectAllSendCalls(SenderMock& sender) {
    EXPECT_CALL(sender, send(_, _))
        .WillRepeatedly(
            [this](const char* data, int size) { appendSentHtml(data, size); });
  }

  void resetRegisterDevice() {
    Supla::RegisterDevice::resetToDefaults();
  }

  std::string sendHtml;
};

class SecurityLoggerStub : public Supla::Device::SecurityLogger {
 public:
  explicit SecurityLoggerStub(std::vector<Supla::SecurityLogEntry> entries)
      : entries_(std::move(entries)) {
  }

  bool prepareGetLog() override {
    nextIndex_ = 0;
    return true;
  }

  Supla::SecurityLogEntry* getLog() override {
    if (nextIndex_ >= entries_.size()) {
      return nullptr;
    }
    return &entries_[nextIndex_++];
  }

  bool isEnabled() const override {
    return true;
  }

  void storeLog(const Supla::SecurityLogEntry&) override {
  }

 private:
  std::vector<Supla::SecurityLogEntry> entries_;
  size_t nextIndex_ = 0;
};

class PwmFrequencyStub : public Supla::Control::RGBCCTBase {
 public:
  PwmFrequencyStub() = default;

  void setRGBCCTValueOnDevice(uint32_t[5], int) override {
  }
};

class BinarySensorStub : public Supla::Sensor::BinaryBase {
 public:
  BinarySensorStub() {
    setServerInvertLogic(true, true);
    setFilteringTimeMs(2500, true);
    setTimeoutDs(42, true);
    setSensitivity(12, true);
  }

  bool getValue() override {
    return false;
  }
};

static Supla::SecurityLogEntry makeSecurityLogEntry(uint32_t index,
                                                    uint32_t timestamp,
                                                    uint32_t source,
                                                    const char* message) {
  Supla::SecurityLogEntry entry;
  std::memset(&entry, 0, sizeof(entry));
  entry.index = index;
  entry.timestamp = timestamp;
  entry.source = source;
  std::snprintf(entry.log, sizeof(entry.log), "%s", message ? message : "");
  return entry;
}

TEST_F(HtmlCaptureTest, DivBeginAndEndGeneratePair) {
  SenderMock sender;
  expectAllSendCalls(sender);

  {
    Supla::Html::DivBegin div("form-field", "box-id");
    div.send(&sender);
  }

  Supla::Html::DivEnd end;
  end.send(&sender);

  EXPECT_EQ(sendHtml, "<div class=\"form-field\" id=\"box-id\"></div>");
}

TEST_F(HtmlCaptureTest, H2AndH3TagsGeneratePairedHeadings) {
  SenderMock sender;
  expectAllSendCalls(sender);

  Supla::Html::H2Tag h2("Main title");
  h2.send(&sender);

  Supla::Html::H3Tag h3("Section title");
  h3.send(&sender);

  EXPECT_EQ(sendHtml, "<h2>Main title</h2><h3>Section title</h3>");
}

TEST_F(HtmlCaptureTest, ButtonRefreshGeneratesRefreshButton) {
  SenderMock sender;
  expectAllSendCalls(sender);

  Supla::Html::ButtonRefresh button;
  button.send(&sender);

  EXPECT_EQ(sendHtml,
            "<button type=\"button\" onclick=\"location.reload();\">"
            "REFRESH"
            "</button>");
}

TEST_F(HtmlCaptureTest, ChannelCorrectionRendersNumberInputAndLabel) {
  NiceMock<ConfigMock> cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getInt32(StrEq("corr_7_1"), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 123;
        return true;
      });

  expectAllSendCalls(sender);

  Supla::Html::ChannelCorrection correction(7, "Living room", 1);
  correction.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("#7 Living room correction"));
  EXPECT_THAT(sendHtml, HasSubstr("for=\"corr_7_1\""));
  EXPECT_THAT(sendHtml, HasSubstr("type=\"number\""));
  EXPECT_THAT(sendHtml, HasSubstr("min=\"-50\""));
  EXPECT_THAT(sendHtml, HasSubstr("max=\"50\""));
  EXPECT_THAT(sendHtml, HasSubstr("step=\"0.1\""));
  EXPECT_THAT(sendHtml, HasSubstr("name=\"corr_7_1\""));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"corr_7_1\""));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"12.3\""));
}

TEST_F(HtmlCaptureTest, ChannelCorrectionHandleResponseStoresCorrection) {
  NiceMock<ConfigMock> cfg;
  Supla::Html::ChannelCorrection correction(7, "Living room", 1);

  EXPECT_CALL(cfg, getInt32(StrEq("corr_7_1"), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, setInt32(StrEq("corr_7_1"), 123)).Times(1);

  EXPECT_TRUE(correction.handleResponse("corr_7_1", "12.3"));
}

TEST_F(HtmlCaptureTest, CustomParameterTemplateRendersFloatingField) {
  NiceMock<ConfigMock> cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getInt32(StrEq("temp_limit"), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 275;
        return true;
      });

  expectAllSendCalls(sender);

  Supla::Html::CustomParameterTemplate<float> param(
      "temp_limit", "Temperature limit", 1.25f, 0.5f, 3.0f, 2);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Temperature limit"));
  EXPECT_THAT(sendHtml, HasSubstr("type=\"number\""));
  EXPECT_THAT(sendHtml, HasSubstr("step=\"0.01\""));
  EXPECT_THAT(sendHtml, HasSubstr("min=\"0.5\""));
  EXPECT_THAT(sendHtml, HasSubstr("max=\"3\""));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"2.75\""));
}

TEST_F(HtmlCaptureTest, CustomParameterTemplateHandlesResponseAndSavesValue) {
  NiceMock<ConfigMock> cfg;
  Supla::Html::CustomParameterTemplate<int32_t> param(
      "gain", "Gain", 5, -10, 10);

  EXPECT_CALL(cfg, init()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setInt32(StrEq("gain"), 8)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(1000)).Times(1);

  EXPECT_TRUE(param.handleResponse("gain", "8"));
}

TEST_F(HtmlCaptureTest, HideShowContainerGeneratesToggledSection) {
  SenderMock sender;
  expectAllSendCalls(sender);

  Supla::Html::HideShowContainerBegin begin("Hidden section");
  begin.send(&sender);
  Supla::Html::HideShowContainerEnd end;
  end.send(&sender);

  ASSERT_THAT(sendHtml, HasSubstr("Show Hidden section"));

  const std::string prefix = "<div id=\"";
  const size_t prefixPos = sendHtml.find(prefix);
  ASSERT_NE(prefixPos, std::string::npos);

  const size_t linkPos = sendHtml.find("_link\">", prefixPos);
  ASSERT_NE(linkPos, std::string::npos);

  const std::string id = sendHtml.substr(prefixPos + prefix.size(),
                                         linkPos - (prefixPos + prefix.size()));
  ASSERT_FALSE(id.empty());

  EXPECT_THAT(sendHtml,
              HasSubstr("document.getElementById(&quot;" + id + "&quot;)"));
  EXPECT_THAT(
      sendHtml,
      HasSubstr("document.getElementById(&quot;" + id + "_link&quot;)"));
  EXPECT_THAT(sendHtml,
              HasSubstr("<div id=\"" + id + "\" style=\"display:none\">"));
}

TEST_F(HtmlCaptureTest, SecurityLogListWithoutLoggerShowsEmptyState) {
  SenderMock sender;
  expectAllSendCalls(sender);

  Supla::Html::SecurityLogList list(nullptr);
  list.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"card\">"
            "<header>"
            "<h1>Security log</h1>"
            "</header>"
            "<p>No security log</p>"
            "</div>");
}

TEST_F(HtmlCaptureTest, SecurityLogListWithEntriesRendersTableRows) {
  SenderMock sender;
  expectAllSendCalls(sender);

  std::vector<Supla::SecurityLogEntry> entries = {
      makeSecurityLogEntry(
          7,
          1234,
          static_cast<uint32_t>(Supla::SecurityLogSource::LOCAL_DEVICE),
          "Door opened"),
  };
  SecurityLoggerStub logger(std::move(entries));

  Supla::Html::SecurityLogList list(&logger);
  list.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"card\">"
            "<header>"
            "<h1>Security log</h1>"
            "</header>"
            "<main>"
            "<table aria-label=\"Security log\">"
            "<thead>"
            "<tr>"
            "<th class=\"col-num\">#</th>"
            "<th class=\"col-ts\">Timestamp</th>"
            "<th class=\"col-src\">Source</th>"
            "<th class=\"col-msg\">Message</th>"
            "</tr>"
            "</thead>"
            "<tbody id=\"log-body\">"
            "<tr><td>7</td><td>1234 s (since boot)</td><td>Local</td>"
            "<td>Door opened</td></tr>"
            "</tbody>"
            "</table>"
            "</main>"
            "</div>");
}

TEST_F(HtmlCaptureTest, CustomCheckboxParameterRendersCheckedState) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getUInt8(StrEq("feature_x"), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::CustomCheckboxParameter param("feature_x", "Feature X", 0);
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field right-checkbox\">"
            "<label for=\"feature_x\">Feature X</label>"
            "<label>"
            "<span class=\"switch\">"
            "<input type=\"checkbox\" value=\"on\" checked name=\"feature_x\" "
            "id=\"feature_x\">"
            "<span class=\"slider\"></span>"
            "</span>"
            "</label>"
            "</div>");
}

TEST_F(HtmlCaptureTest, ButtonHoldTimeParametersUseDefaultFallback) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getUInt32(StrEq(Supla::ConfigTag::BtnHoldTag), _))
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ButtonHoldTimeParameters param;
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"btn_hold\">Hold detection time [s]</label>"
            "<input type=\"number\" min=\"0.2\" max=\"10\" step=\"0.1\" "
            "name=\"btn_hold\" id=\"btn_hold\" value=\"0.7\">"
            "</div>");
}

TEST_F(HtmlCaptureTest, ButtonMulticlickParametersUseDefaultFallback) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getUInt32(StrEq(Supla::ConfigTag::BtnMulticlickTag), _))
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ButtonMulticlickParameters param;
  param.send(&sender);

  EXPECT_EQ(
      sendHtml,
      "<div class=\"form-field\">"
      "<label for=\"btn_multiclick\">Multiclick detection time [s]</label>"
      "<input type=\"number\" min=\"0.2\" max=\"10\" step=\"0.1\" "
      "name=\"btn_multiclick\" id=\"btn_multiclick\" value=\"0.3\">"
      "</div>");
}

TEST_F(HtmlCaptureTest, VolumeParametersRendersRangeInput) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getUInt8(StrEq(Supla::ConfigTag::VolumeCfgTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 42;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::VolumeParameters param;
  param.send(&sender);

  EXPECT_EQ(
      sendHtml,
      "<div class=\"form-field\">"
      "<label for=\"volume\">Button volume</label>"
      "<input type=\"range\" class=\"range-slider\" min=\"0\" max=\"100\" "
      "step=\"1\" name=\"volume\" id=\"volume\" value=\"42\">"
      "</div>");
}

TEST_F(HtmlCaptureTest, StatusLedParametersRendersSelectedOption) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getInt8(StrEq(Supla::ConfigTag::StatusLedCfgTag), _))
      .WillOnce([](const char*, int8_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::StatusLedParameters param;
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"led\">Status LED</label>"
            "<select name=\"led\" id=\"led\">"
            "<option value=\"0\">ON - WHEN CONNECTED</option>"
            "<option value=\"1\" selected>OFF - WHEN CONNECTED</option>"
            "<option value=\"2\">ALWAYS OFF</option>"
            "</select>"
            "</div>");
}

TEST_F(HtmlCaptureTest, CustomSwUpdateRendersUrlInput) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillOnce([](char* url) {
    std::snprintf(
        url, SUPLA_MAX_URL_LENGTH, "%s", "https://updates.example.com");
    return true;
  });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::CustomSwUpdate param;
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"swupdateurl\">Update server address</label>"
            "<input type=\"text\" name=\"swupdateurl\" id=\"swupdateurl\" "
            "value=\"https://updates.example.com\">"
            "</div>");
}

TEST_F(HtmlCaptureTest, ButtonTypeParametersRendersAllOptions) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(key, 4, Supla::ConfigTag::BtnTypeTag);

  EXPECT_CALL(cfg, getInt32(StrEq(key), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 3;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ButtonTypeParameters param(4, "Room");
  param.addDefaultOptions();
  param.addCentralControlOption();
  param.send(&sender);

  EXPECT_EQ(
      sendHtml,
      "<div class=\"form-field\">"
      "<label for=\"4_btn_type\">Room type</label>"
      "<select name=\"4_btn_type\" id=\"4_btn_type\">"
      "<option value=\"MONOSTABLE\">MONOSTABLE</option>"
      "<option value=\"BISTABLE\">BISTABLE</option>"
      "<option value=\"MOTION SENSOR\">MOTION SENSOR</option>"
      "<option value=\"CENTRAL CONTROL\" selected>CENTRAL CONTROL</option>"
      "</select>"
      "</div>");
}

TEST_F(HtmlCaptureTest, ButtonActionTriggerConfigRendersSelectedOption) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(
      key, 3, Supla::ConfigTag::BtnActionTriggerCfgTagPrefix);

  EXPECT_CALL(cfg, getInt32(StrEq(key), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ButtonActionTriggerConfig param(3, 2);
  param.send(&sender);

  EXPECT_EQ(
      sendHtml,
      "<div class=\"form-field\">"
      "<label for=\"3_mqtt_at\">IN2 MQTT action trigger type</label>"
      "<select name=\"3_mqtt_at\" id=\"3_mqtt_at\">"
      "<option value=\"0\">Publish based on Supla Cloud config</option>"
      "<option value=\"1\" selected>Publish all triggers, don&apos;t disable "
      "local function</option>"
      "<option value=\"2\">Publish all triggers, disable local "
      "function</option>"
      "</select>"
      "</div>");
}

TEST_F(HtmlCaptureTest, SwUpdateBetaRendersSelectedState) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getDeviceMode())
      .WillOnce(Return(Supla::DEVICE_MODE_SW_UPDATE));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillOnce(Return(true));
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::SwUpdateBeta param;
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"updbeta\">Firmware update</label>"
            "<select name=\"updbeta\" id=\"updbeta\">"
            "<option value=\"0\">NO</option>"
            "<option value=\"1\">YES</option>"
            "<option value=\"2\" selected>YES - BETA</option>"
            "</select>"
            "<div class=\"hint\">Warning: beta SW versions may contain bugs "
            "and your device may not work properly.</div>"
            "</div>");
}

TEST_F(HtmlCaptureTest, ProtocolParametersRendersSimpleProtocolSelector) {
  ::testing::NiceMock<ConfigMock> cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillOnce(Return(true));
  EXPECT_CALL(cfg, isMqttCommProtocolEnabled()).WillOnce(Return(false));
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ProtocolParameters param(true, false);
  param.send(&sender);

  EXPECT_THAT(sendHtml, StartsWith("<div class=\"box\">"));
  EXPECT_THAT(sendHtml, HasSubstr("<label for=\"pro\">Protocol</label>"));
  EXPECT_THAT(sendHtml, HasSubstr("<option value=\"0\""));
  EXPECT_THAT(sendHtml, HasSubstr(">Supla</option>"));
  EXPECT_THAT(sendHtml, HasSubstr("<option value=\"1\""));
  EXPECT_THAT(sendHtml, HasSubstr(">MQTT</option>"));
  EXPECT_THAT(sendHtml, HasSubstr("onchange=\"protocolChanged()\""));
}

TEST_F(HtmlCaptureTest, SwUpdateRendersSimpleFirmwareSelector) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getDeviceMode()).WillOnce(Return(Supla::DEVICE_MODE_NORMAL));
  EXPECT_CALL(cfg, getInt8(StrEq("swUpdNoCert"), _))
      .WillOnce([](const char*, int8_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::SwUpdate param(nullptr);
  param.send(&sender);

  EXPECT_THAT(sendHtml,
              HasSubstr("<label for=\"upd\">Firmware update</label>"));
  EXPECT_THAT(sendHtml, HasSubstr("<option value=\"0\" selected>NO</option>"));
  EXPECT_THAT(sendHtml, HasSubstr("<option value=\"1\">YES</option>"));
  EXPECT_THAT(sendHtml,
              HasSubstr("<option value=\"2\">YES - SKIP CERTIFICATE "
                        "(dangerous)</option>"));
  EXPECT_THAT(sendHtml, HasSubstr("<select "));
}

TEST_F(HtmlCaptureTest, PowerStatusLedParametersRendersSelectedOption) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getInt8(StrEq(Supla::ConfigTag::PowerStatusLedCfgTag), _))
      .WillOnce([](const char*, int8_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::PowerStatusLedParameters param;
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"pwr_led\">Power Status LED</label>"
            "<select name=\"pwr_led\" id=\"pwr_led\">"
            "<option value=\"0\">Enabled</option>"
            "<option value=\"1\" selected>Disabled</option>"
            "</select>"
            "</div>");
}

TEST_F(HtmlCaptureTest, ScreenDelayParametersRendersNumberInput) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getInt32(StrEq(Supla::ConfigTag::ScreenDelayCfgTag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 120;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ScreenDelayParameters param;
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"scr_delay\">Turn screen off after [sec]</label>"
            "<input type=\"number\" min=\"0\" max=\"65535\" step=\"1\" "
            "name=\"scr_delay\" id=\"scr_delay\" value=\"120\">"
            "</div>");
}

TEST_F(HtmlCaptureTest, ButtonConfigParametersRendersOnOffOptions) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(key, 3, Supla::ConfigTag::BtnConfigTag);

  EXPECT_CALL(cfg, getInt32(StrEq(key), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ButtonConfigParameters param(3);
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"3_btn_cfg\">IN3 Config</label>"
            "<select name=\"3_btn_cfg\" id=\"3_btn_cfg\">"
            "<option value=\"ON\">ON</option>"
            "<option value=\"OFF\" selected>OFF</option>"
            "</select>"
            "</div>");
}

TEST_F(HtmlCaptureTest, DisableUserInterfaceParameterRendersRestrictionBox) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg,
              getUInt8(StrEq(Supla::ConfigTag::DisableUserInterfaceCfgTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 2;
        return true;
      });
  EXPECT_CALL(cfg, getInt32(StrEq(Supla::ConfigTag::MinTempUICfgTag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 1234;
        return true;
      });
  EXPECT_CALL(cfg, getInt32(StrEq(Supla::ConfigTag::MaxTempUICfgTag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 4321;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::DisableUserInterfaceParameter param;
  param.send(&sender);

  EXPECT_THAT(
      sendHtml,
      HasSubstr(
          "<label for=\"disable_ui\">Local interface restriction</label>"));
  EXPECT_THAT(
      sendHtml,
      HasSubstr(
          "<option value=\"2\" selected>Allow temperature change</option>"));
  EXPECT_THAT(
      sendHtml,
      HasSubstr("<div id=\"min_max_temp_ui\" style=\"display: block\">"));
  EXPECT_THAT(
      sendHtml,
      HasSubstr(
          "<label for=\"min_temp_ui\">Interface minimum temperature</label>"));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"12.3\""));
  EXPECT_THAT(
      sendHtml,
      HasSubstr(
          "<label for=\"max_temp_ui\">Interface maximum temperature</label>"));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"43.2\""));
}

TEST_F(HtmlCaptureTest, HomeScreenContentRendersConfiguredOptions) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getInt8(StrEq(Supla::ConfigTag::HomeScreenContentTag), _))
      .WillOnce([](const char*, int8_t* value) {
        *value = 4;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::HomeScreenContentParameters param("Home screen content");
  param.initFields(SUPLA_DEVCFG_HOME_SCREEN_CONTENT_NONE |
                   SUPLA_DEVCFG_HOME_SCREEN_CONTENT_TEMPERATURE |
                   SUPLA_DEVCFG_HOME_SCREEN_CONTENT_TIME_DATE);
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"home_screen\">Home screen content</label>"
            "<select name=\"home_screen\" id=\"home_screen\">"
            "<option value=\"None\">None</option>"
            "<option value=\"Temperature\">Temperature</option>"
            "<option value=\"Time and date\" selected>Time and date</option>"
            "</select>"
            "</div>");
}

TEST_F(HtmlCaptureTest, RelayParametersRendersThresholdInput) {
  SenderMock sender;
  sendHtml.clear();

  Supla::Control::Relay relay(nullptr, 7);
  relay.setOvercurrentMaxAllowed(5678);
  relay.setOvercurrentThreshold(1234);

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::RelayParameters param(&relay);
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"0_oc_thr\">Overcurrent protection [A]</label>"
            "<input type=\"number\" min=\"0\" max=\"56.78\" step=\"0.01\" "
            "name=\"0_oc_thr\" id=\"0_oc_thr\" value=\"12.34\">"
            "</div>");
}

TEST_F(HtmlCaptureTest, TimeParametersRendersClockControls) {
  ConfigMock cfg;
  SenderMock sender;
  ClockMock clock;
  sendHtml.clear();

  EXPECT_CALL(cfg, getUInt8(StrEq(Supla::AutomaticTimeSyncCfgTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  SuplaDeviceClass sdc;
  Supla::Html::TimeParameters param(&sdc);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Automatic time sync"));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"time_settings_box\""));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"time_settings_inner_box\""));
  EXPECT_THAT(sendHtml, HasSubstr("id=\"date_time_value\""));
  EXPECT_THAT(sendHtml, HasSubstr("function showHideTimeSettingsToggle()"));
}

TEST_F(HtmlCaptureTest, EthernetParametersRendersCheckbox) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getUInt8(StrEq(Supla::EthDisableTag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::EthernetParameters param;
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<h3>Ethernet Settings</h3>"
            "<div class=\"form-field right-checkbox\">"
            "<label for=\"eth_en\">Enable Ethernet</label>"
            "<label>"
            "<span class=\"switch\">"
            "<input type=\"checkbox\" value=\"on\" checked name=\"eth_en\" "
            "id=\"eth_en\">"
            "<span class=\"slider\"></span>"
            "</span>"
            "</label>"
            "</div>");
}

TEST_F(HtmlCaptureTest, PwmFrequencyParametersRendersDefaultRange) {
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  PwmFrequencyStub pwm;
  Supla::Html::PwmFrequencyParameters param(&pwm);
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"pwm_freq\">PWM frequency [Hz] (reboot required to "
            "take effect)</label>"
            "<input type=\"number\" min=\"100\" max=\"9000\" step=\"1\" "
            "name=\"pwm_freq\" id=\"pwm_freq\" value=\"500\">"
            "</div>");
}

TEST_F(HtmlCaptureTest, RgbwButtonParametersRendersDefaultOptions) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, getInt32(StrEq(Supla::ConfigTag::RgbwButtonTag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 2;
        return true;
      });
  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::RgbwButtonParameters param;
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"rgbw_btn\">RGBW output controlled by IN</label>"
            "<select name=\"rgbw_btn\" id=\"rgbw_btn\">"
            "<option value=\"RGB+W\">RGB+W</option>"
            "<option value=\"RGB\">RGB</option>"
            "<option value=\"W\" selected>W</option>"
            "<option value=\"NONE\">NONE</option>"
            "</select>"
            "</div>");
}

TEST_F(HtmlCaptureTest, TextCmdInputParameterRendersTextInput) {
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::TextCmdInputParameter param("text_cmd", "Text command");
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"text_cmd\">Text command</label>"
            "<input type=\"text\" name=\"text_cmd\" id=\"text_cmd\">"
            "</div>");
}

TEST_F(HtmlCaptureTest, SelectCmdInputParameterRendersSelectInput) {
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::SelectCmdInputParameter param("select_cmd", "Select command");
  param.registerCmd("one", 1);
  param.registerCmd("two", 2);
  param.send(&sender);

  EXPECT_EQ(sendHtml,
            "<div class=\"form-field\">"
            "<label for=\"select_cmd\">Select command</label>"
            "<select name=\"select_cmd\" id=\"select_cmd\">"
            "<option selected></option>"
            "<option value=\"one\">one</option>"
            "<option value=\"two\">two</option>"
            "</select>"
            "</div>");
}

TEST_F(HtmlCaptureTest, BinarySensorParametersRendersFields) {
  SenderMock sender;
  sendHtml.clear();

  Supla::Channel::resetToDefaults();
  BinarySensorStub binary;

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::BinarySensorParameters param(&binary);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Binary sensor #"));
  EXPECT_THAT(sendHtml, HasSubstr("Inverted logic"));
  EXPECT_THAT(sendHtml, HasSubstr("Filtering time [s]"));
  EXPECT_THAT(sendHtml, HasSubstr("Sensor timeout [s]"));
  EXPECT_THAT(sendHtml, HasSubstr("Sensor sensitivity [%]"));
}

TEST_F(HtmlCaptureTest, ScreenDelayTypeParametersRendersSelectedOption) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(cfg, getInt32(StrEq(Supla::ConfigTag::ScreenDelayTypeCfgTag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ScreenDelayTypeParameters param;
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Automatic screen off type"));
  EXPECT_THAT(sendHtml, HasSubstr("name=\"scr_delay_t\" id=\"scr_delay_t\""));
  EXPECT_THAT(sendHtml,
              HasSubstr("value=\"Enabled only when it&apos;s dark\""));
  EXPECT_THAT(sendHtml, HasSubstr("Enabled only when it&apos;s dark</option>"));
}

TEST_F(HtmlCaptureTest, EmCtTypeParametersRendersSupportedOptions) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(cfg, getInt32(StrEq("0_em_ct"), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Sensor::ElectricityMeter em;
  em.addCtType(EM_CT_TYPE_100A_33mA);
  em.addCtType(EM_CT_TYPE_200A_66mA);

  Supla::Html::EmCtTypeParameters param(&em);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Current transformer"));
  EXPECT_THAT(sendHtml, HasSubstr("name=\"0_em_ct\" id=\"0_em_ct\""));
  EXPECT_THAT(sendHtml,
              HasSubstr("<option value=\"100A/33.3mA\">100A/33.3mA</option>"));
  EXPECT_THAT(
      sendHtml,
      HasSubstr("<option value=\"200A/66.6mA\" selected>200A/66.6mA</option>"));
}

TEST_F(HtmlCaptureTest, ScreenBrightnessParametersRendersAutomaticMode) {
  ConfigMock cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(cfg, getInt32(StrEq(Supla::ConfigTag::ScreenBrightnessCfgTag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = -1;
        return true;
      });
  EXPECT_CALL(
      cfg,
      getInt32(StrEq(Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = -30;
        return true;
      });
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ScreenBrightnessParameters param;
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Automatic screen brightness"));
  EXPECT_THAT(sendHtml, HasSubstr("showHideBrightnessSettingsToggle()"));
  EXPECT_THAT(sendHtml, HasSubstr("adjustment_auto_box"));
  EXPECT_THAT(sendHtml, HasSubstr("brightness_settings_box"));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"-30\""));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"100\""));
}

TEST_F(HtmlCaptureTest, EmPhaseLedParametersRendersSupportedOptions) {
  NiceMock<ConfigMock> cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(cfg, getInt32(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Sensor::ElectricityMeter em;
  em.addPhaseLedType(EM_PHASE_LED_TYPE_OFF);
  em.addPhaseLedType(EM_PHASE_LED_TYPE_VOLTAGE_PRESENCE);
  em.addPhaseLedType(EM_PHASE_LED_TYPE_VOLTAGE_LEVEL);

  Supla::Html::EmPhaseLedParameters param(&em);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Phase indicator LED"));
  EXPECT_THAT(sendHtml, HasSubstr("onChange=\"emPhaseChange(this.value)\""));
  EXPECT_THAT(sendHtml, HasSubstr("<option value=\"0\" selected>OFF</option>"));
  EXPECT_THAT(sendHtml,
              HasSubstr("<option value=\"1\">Voltage indicator</option>"));
  EXPECT_THAT(sendHtml,
              HasSubstr("<option value=\"3\">Voltage level</option>"));
  EXPECT_THAT(sendHtml, HasSubstr("em_phase_voltage"));
  EXPECT_THAT(sendHtml, HasSubstr("em_phase_power"));
}

TEST_F(HtmlCaptureTest, ModbusParametersRendersSerialAndNetworkSelectors) {
  SenderMock sender;
  sendHtml.clear();

  Supla::Modbus::Configurator modbus;
  Supla::Modbus::ConfigProperties properties = {};
  properties.protocol.master = 1;
  properties.protocol.slave = 1;
  properties.protocol.rtu = 1;
  properties.protocol.ascii = 1;
  properties.protocol.tcp = 1;
  properties.protocol.udp = 1;
  properties.baudrate.b4800 = 1;
  properties.baudrate.b9600 = 1;
  properties.baudrate.b19200 = 1;
  properties.baudrate.b38400 = 1;
  properties.baudrate.b57600 = 1;
  properties.baudrate.b115200 = 1;
  properties.stopBits.one = 1;
  properties.stopBits.oneAndHalf = 1;
  properties.stopBits.two = 1;
  modbus.setProperties(properties);

  Supla::Modbus::Config config = {};
  config.role = Supla::Modbus::Role::Master;
  config.modbusAddress = 12;
  config.serial.mode = Supla::Modbus::ModeSerial::Ascii;
  config.serial.baudrate = 57600;
  config.serial.stopBits = Supla::Modbus::SerialStopBits::Two;
  config.network.mode = Supla::Modbus::ModeNetwork::Tcp;
  config.network.port = 1502;
  modbus.setConfig(config);

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::ModbusParameters param(&modbus);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Modbus Settings"));
  EXPECT_THAT(sendHtml, HasSubstr("Modbus mode"));
  EXPECT_THAT(sendHtml, HasSubstr("Modbus address (only for Slave)"));
  EXPECT_THAT(sendHtml, HasSubstr("Modbus serial mode"));
  EXPECT_THAT(sendHtml, HasSubstr("Baudrate"));
  EXPECT_THAT(sendHtml, HasSubstr("Stop bits"));
  EXPECT_THAT(sendHtml, HasSubstr("Modbus network mode"));
  EXPECT_THAT(
      sendHtml,
      HasSubstr("<option value=\"1\" selected>Master (client)</option>"));
  EXPECT_THAT(sendHtml,
              HasSubstr("<option value=\"2\">Slave (server)</option>"));
  EXPECT_THAT(sendHtml,
              HasSubstr("<option value=\"2\" selected>ASCII</option>"));
  EXPECT_THAT(sendHtml,
              HasSubstr("<option value=\"57600\" selected>57600</option>"));
  EXPECT_THAT(sendHtml, HasSubstr("<option value=\"2\" selected>2</option>"));
  EXPECT_THAT(sendHtml, HasSubstr("<option value=\"1\" selected>TCP</option>"));
}

TEST_F(HtmlCaptureTest, RollerShutterParametersRendersBasicFields) {
  NiceMock<ConfigMock> cfg;
  SenderMock sender;
  sendHtml.clear();

  Supla::Control::RollerShutter rs(-1, -1);
  rs.getChannel()->setChannelNumber(7);
  rs.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  rs.setRsConfigMotorUpsideDownEnabled(true);
  rs.setRsConfigMotorUpsideDownValue(2);
  rs.setRsConfigButtonsUpsideDownEnabled(true);
  rs.setRsConfigButtonsUpsideDownValue(1);
  rs.setRsConfigTimeMarginEnabled(true);
  rs.setRsConfigTimeMarginValue(5);
  rs.setOpenCloseTime(45600, 12300);

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Html::RollerShutterParameters param(&rs);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Roller shutter #7"));
  EXPECT_THAT(sendHtml, HasSubstr("Channel function"));
  EXPECT_THAT(sendHtml, HasSubstr("Motor upside down"));
  EXPECT_THAT(sendHtml, HasSubstr("Buttons upside down"));
  EXPECT_THAT(sendHtml, HasSubstr("Time margin (%)"));
  EXPECT_THAT(sendHtml, HasSubstr("Full opening time (sec.)"));
  EXPECT_THAT(sendHtml, HasSubstr("Full closing time (sec.)"));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"12.3\""));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"45.6\""));
}

TEST_F(HtmlCaptureTest, HvacParametersRendersBasicThermostatFields) {
  NiceMock<ConfigMock> cfg;
  SenderMock sender;
  OutputSimulatorWithCheck output;
  SimpleTime time;
  sendHtml.clear();

  (void)cfg;
  (void)time;

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Control::HvacBase hvac(&output);
  hvac.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
  hvac.getChannel()->setHvacMode(SUPLA_HVAC_MODE_HEAT_COOL);
  hvac.getChannel()->setHvacSetpointTemperatureHeat(2100);
  hvac.getChannel()->setHvacSetpointTemperatureCool(2500);

  Supla::Html::HvacParameters param(&hvac);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Thermostat #0"));
  EXPECT_THAT(sendHtml, HasSubstr("Channel function"));
  EXPECT_THAT(sendHtml, HasSubstr("Heat + cool"));
  EXPECT_THAT(sendHtml, HasSubstr("Mode"));
  EXPECT_THAT(sendHtml, HasSubstr("Heating temperature setpoint [°C]"));
  EXPECT_THAT(sendHtml, HasSubstr("Cooling temperature setpoint [°C]"));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"21.0\""));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"25.0\""));
}

TEST_F(HtmlCaptureTest, ContainerParametersRendersBasicFields) {
  NiceMock<ConfigMock> cfg;
  SenderMock sender;
  sendHtml.clear();

  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly(
          [this](const char* data, int size) { appendSentHtml(data, size); });

  Supla::Sensor::Container container;
  container.setWarningAboveLevel(70);
  container.setAlarmAboveLevel(90);
  container.setWarningBelowLevel(30);
  container.setAlarmBelowLevel(10);
  container.setMuteAlarmSoundWithoutAdditionalAuth(true);

  Supla::Html::ContainerParameters param(false, &container);
  param.send(&sender);

  EXPECT_THAT(sendHtml, HasSubstr("Container #0"));
  EXPECT_THAT(sendHtml, HasSubstr("Warning above level"));
  EXPECT_THAT(sendHtml, HasSubstr("Alarm above level"));
  EXPECT_THAT(sendHtml, HasSubstr("Warning below level"));
  EXPECT_THAT(sendHtml, HasSubstr("Alarm below level"));
  EXPECT_THAT(sendHtml, HasSubstr("Mute alarm sound without additional auth"));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"70\""));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"90\""));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"30\""));
  EXPECT_THAT(sendHtml, HasSubstr("value=\"10\""));
}

TEST_F(HtmlCaptureTest, DeviceInfoRendersRegisterDeviceAndMainMac) {
  resetRegisterDevice();

  SenderMock sender;
  expectAllSendCalls(sender);
  SuplaDeviceClass sdc;

  char guid[SUPLA_GUID_SIZE] = {};
  for (int i = 0; i < SUPLA_GUID_SIZE; i++) {
    guid[i] = static_cast<char>(i);
  }
  Supla::RegisterDevice::setName("Test device");
  Supla::RegisterDevice::setSoftVer("1.2.3");
  Supla::RegisterDevice::setGUID(guid);

  Supla::Html::DeviceInfo info(&sdc);
  info.send(&sender);
  resetRegisterDevice();

  EXPECT_EQ(sendHtml,
            "<h1>Test device</h1><span>"
            "<br>Firmware: 1.2.3"
            "<br>GUID: 000102030405060708090A0B0C0D0E0F"
            "</span><span>"
            "<br>Uptime: 0 s"
            "</span>");
}
