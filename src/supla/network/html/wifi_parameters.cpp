// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "wifi_parameters.h"

#include <stdio.h>
#include <string.h>

#include <supla/network/network.h>
#include <supla/network/netif_wifi.h>
#include <supla/network/web_sender.h>
#include <supla/network/web_server.h>
#include <supla/network/wifi_scan_result.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/tools.h>

namespace Supla {

namespace Html {

namespace {

constexpr char WifiSsidListId[] = "wifi_ssid_list";
constexpr char WifiSsidHintId[] = "wifi_scan_hint";
constexpr char WifiSsidChanged[] = "wifiSsidChanged()";

bool hasFreshWifiScanResults() {
  auto cache = WifiScanResultCache::Instance();
  return cache != nullptr &&
         cache->hasScan() &&
         cache->getCount() > 0 &&
         millis() - cache->getTimestampMs() <= WifiScanDefaultMaxAgeMs;
}

void formatWifiScanMessage(char *message,
                           size_t messageSize,
                           const char *ssid,
                           bool includeSsid) {
  if (message == nullptr || messageSize == 0) {
    return;
  }
  message[0] = '\0';

  WifiScanResult result = {};
  auto status = WifiScanResultCache::Instance()->lookup(
      ssid, &result, millis(), WifiScanDefaultMaxAgeMs);

  switch (status) {
    case WifiScanLookupStatus::Found: {
      if (includeSsid) {
        snprintf(message,
                 messageSize,
                 "Wi-Fi: \"%s\" found, RSSI %d dBm, quality %d%%",
                 ssid,
                 static_cast<int>(result.rssi),
                 Supla::rssiToSignalStrength(result.rssi));
      } else {
        snprintf(message,
                 messageSize,
                 "Found in Wi-Fi scan: RSSI %d dBm, quality %d%%",
                 static_cast<int>(result.rssi),
                 Supla::rssiToSignalStrength(result.rssi));
      }
      break;
    }
    case WifiScanLookupStatus::NotFound: {
      if (includeSsid) {
        snprintf(message,
                 messageSize,
                 "Wi-Fi: \"%s\" was not found in the latest scan",
                 ssid);
      } else {
        snprintf(
            message,
            messageSize,
            "Warning: this network was not found in the latest Wi-Fi scan");
      }
      break;
    }
    case WifiScanLookupStatus::Stale: {
      snprintf(message, messageSize, "Wi-Fi scan result is no longer fresh");
      break;
    }
    case WifiScanLookupStatus::NotAvailable:
    default: {
      snprintf(message, messageSize, "Wi-Fi scan result is not available yet");
      break;
    }
  }
}

const char *wifiScanHintClass(const char *ssid) {
  WifiScanResult result = {};
  auto status = WifiScanResultCache::Instance()->lookup(
      ssid, &result, millis(), WifiScanDefaultMaxAgeMs);

  if (status == WifiScanLookupStatus::NotFound) {
    return "hint warn";
  }

  if (status != WifiScanLookupStatus::Found) {
    return "hint";
  }

  int quality = Supla::rssiToSignalStrength(result.rssi);
  if (quality <= 20) {
    return "hint error";
  }
  if (quality <= 35) {
    return "hint warn";
  }
  return "hint";
}

void sendJsString(Supla::WebSender *sender, const char *text) {
  sender->send("'");
  if (text != nullptr) {
    for (const char *ptr = text; *ptr != '\0'; ptr++) {
      switch (*ptr) {
        case '\\':
          sender->send("\\\\");
          break;
        case '\'':
          sender->send("\\'");
          break;
        case '\n':
          sender->send("\\n");
          break;
        case '\r':
          sender->send("\\r");
          break;
        case '<':
          sender->send("\\x3C");
          break;
        case '>':
          sender->send("\\x3E");
          break;
        case '&':
          sender->send("\\x26");
          break;
        default:
          sender->send(ptr, 1);
          break;
      }
    }
  }
  sender->send("'");
}

void sendWifiScanHint(Supla::WebSender *sender, const char *ssid) {
  auto hint = sender->tag("div");
  hint.attr("class", wifiScanHintClass(ssid));
  hint.attr("id", WifiSsidHintId);
  hint.body([&]() {
    if (ssid != nullptr && ssid[0] != '\0') {
      char message[96] = {};
      formatWifiScanMessage(message, sizeof(message), ssid, false);
      sender->sendSafe(message);
    }
  });
}

void sendWifiSsidDatalist(Supla::WebSender *sender) {
  if (!hasFreshWifiScanResults()) {
    return;
  }

  auto cache = WifiScanResultCache::Instance();
  auto datalist = sender->tag("datalist");
  datalist.attr("id", WifiSsidListId);
  datalist.body([&]() {
    for (uint8_t i = 0; i < cache->getCount(); i++) {
      WifiScanResult result = {};
      if (!cache->getResult(i, &result)) {
        continue;
      }

      sender->selectOption(result.ssid, result.ssid);
    }
  });
}

void sendWifiScanScript(Supla::WebSender *sender) {
  sender->send("<script>"
               "var wifiScanHints=Object.create(null);"
               "var wifiScanHintClasses=Object.create(null);");

  bool freshScan = hasFreshWifiScanResults();
  auto cache = WifiScanResultCache::Instance();
  if (freshScan) {
    for (uint8_t i = 0; i < cache->getCount(); i++) {
      WifiScanResult result = {};
      if (!cache->getResult(i, &result)) {
        continue;
      }

      char message[96] = {};
      snprintf(message,
               sizeof(message),
               "Found in Wi-Fi scan: RSSI %d dBm, quality %d%%",
               static_cast<int>(result.rssi),
               Supla::rssiToSignalStrength(result.rssi));

      sender->send("wifiScanHints[");
      sendJsString(sender, result.ssid);
      sender->send("]=");
      sendJsString(sender, message);
      sender->send(";");
      sender->send("wifiScanHintClasses[");
      sendJsString(sender, result.ssid);
      sender->send("]=");
      sendJsString(sender, wifiScanHintClass(result.ssid));
      sender->send(";");
    }
  }

  sender->send("var wifiScanFresh=");
  sender->send(freshScan ? "true" : "false");
  sender->send(";function wifiSsidChanged(){"
               "var e=document.getElementById('sid');"
               "var h=document.getElementById('wifi_scan_hint');"
               "if(e==null||h==null){return;}"
               "if(e.value==''){h.textContent='';h.className='hint';return;}"
               "if(wifiScanFresh){"
               "if(wifiScanHints[e.value]){"
               "h.textContent=wifiScanHints[e.value];"
               "h.className=wifiScanHintClasses[e.value]||'hint';"
               "}else{"
               "h.textContent="
               "'Warning: this network was not found in the latest Wi-Fi scan';"
               "h.className='hint warn';"
               "}"
               "}else{"
               "h.textContent='Wi-Fi scan result is not available yet';"
               "h.className='hint';"
               "}}"
               "</script>");
}

}  // namespace

WifiParameters::WifiParameters()
    : HtmlElement(HTML_SECTION_NETWORK),
      netifParameters(Supla::ConfigTag::WifiNetifCfgTag, "wifi") {
}

WifiParameters::~WifiParameters() {
}

void WifiParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    sender->tag("h3").body("Wi-Fi Settings");
    char buf[256] = {};

    if (Supla::Network::GetNetIntfCount() > 1) {
      const char wifiEn[] = "wifi_en";  // HTML field
      uint8_t wifiDisabled = 0;
      cfg->getUInt8(Supla::WifiDisableTag, &wifiDisabled);

      sender->labeledField(
          wifiEn,
          "Enable Wi-Fi",
          [&]() {
            auto switchLabel = sender->tag("label");
            switchLabel.body([&]() {
              auto sw = sender->tag("span");
              sw.attr("class", "switch");
              sw.body([&]() {
                sender->checkboxInput(wifiEn, wifiEn, wifiDisabled == 0);

                auto slider = sender->tag("span");
                slider.attr("class", "slider");
                slider.body("");
              });
            });
          },
          "form-field right-checkbox");
    }

    const char key[] = "sid";
    sender->labeledField(key, "Network name", [&]() {
      auto inputAndHint = sender->tag("div");
      inputAndHint.body([&]() {
        const char *listId = hasFreshWifiScanResults() ? WifiSsidListId
                                                       : nullptr;
        if (cfg->getWiFiSSID(buf)) {
          sender->textInput(
              key, key, buf, -1, listId, WifiSsidChanged, WifiSsidChanged);
          sendWifiScanHint(sender, buf);
        } else {
          sender->textInput(
              key, key, nullptr, -1, listId, WifiSsidChanged, WifiSsidChanged);
          sendWifiScanHint(sender, nullptr);
        }
        sendWifiSsidDatalist(sender);
        sendWifiScanScript(sender);
      });
    }, "form-field sensitive");

    const char keyPass[] = "wpw";
    sender->labeledField(keyPass, "Password", [&]() {
      sender->passwordInput(keyPass, keyPass);
    }, "form-field sensitive");

    netifParameters.send(sender);
  }
}

bool WifiParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return netifParameters.handleResponse(key, value);
  }
  if (strcmp(key, "sid") == 0) {
    wifiSettingsSeen = true;
    cfg->setWiFiSSID(value);
    return true;
  } else if (strcmp(key, "wpw") == 0) {
    wifiSettingsSeen = true;
    if (strlen(value) > 0) {
      cfg->setWiFiPassword(value);
    }
    return true;
  } else if (strcmp(key, "wifi_en") == 0) {
    wifiSettingsSeen = true;
    checkboxFound = true;
    uint8_t wifiDisVale = (strncmp(value, "on", 3) == 0 ? 0 : 1);
    cfg->setUInt8(Supla::WifiDisableTag, wifiDisVale);
    return true;
  }

  bool handled = netifParameters.handleResponse(key, value);
  if (handled) {
    wifiSettingsSeen = true;
  }
  return handled;
}

void WifiParameters::onProcessingEnd() {
  if (!checkboxFound && Supla::Network::GetNetIntfCount() > 1) {
    // checkbox doesn't send value when it is not checked, so on processing end
    // we check if it was found earlier, and if not, then we process it as "off"
    handleResponse("wifi_en", "off");
  }
  if (wifiSettingsSeen) {
    logWifiScanResult();
  }
  wifiSettingsSeen = false;
  checkboxFound = false;
  netifParameters.onProcessingEnd();
}

void WifiParameters::logWifiScanResult() {
  auto server = Supla::WebServer::Instance();
  auto cfg = Supla::Storage::ConfigInstance();
  if (server == nullptr || cfg == nullptr) {
    return;
  }

  char ssid[MAX_SSID_SIZE] = {};
  if (!cfg->getWiFiSSID(ssid) || ssid[0] == '\0') {
    return;
  }

  char message[128] = {};
  formatWifiScanMessage(message, sizeof(message), ssid, true);
  if (message[0] == '\0' ||
      strncmp(message, lastLoggedWifiScanMessage, sizeof(message)) == 0) {
    return;
  }

  strncpy(lastLoggedWifiScanMessage, message,
          sizeof(lastLoggedWifiScanMessage) - 1);
  lastLoggedWifiScanMessage[sizeof(lastLoggedWifiScanMessage) - 1] = '\0';
  server->addLastStateLog(message);
}

};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR
