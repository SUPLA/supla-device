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

#include <cstring>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <SuplaDevice.h>
#include <supla/device/register_device.h>
#include <supla/device/security_logger.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/div.h>
#include <supla/network/html/h2_tag.h>
#include <supla/network/html/h3_tag.h>
#include <supla/network/html/hide_show_container.h>
#include <supla/network/html/security_log_list.h>
#include <supla/network/html/button_refresh.h>
#include <supla/network/web_sender.h>

using ::testing::_;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::StartsWith;

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
        .WillRepeatedly([this](const char* data, int size) {
          appendSentHtml(data, size);
        });
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

  EXPECT_THAT(sendHtml, HasSubstr("document.getElementById(\"" + id + "\")"));
  EXPECT_THAT(sendHtml,
              HasSubstr("document.getElementById(\"" + id + "_link\")"));
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
      makeSecurityLogEntry(7,
                           1234,
                           static_cast<uint32_t>(
                               Supla::SecurityLogSource::LOCAL_DEVICE),
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
