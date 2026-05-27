/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef ARDUINO_ARCH_AVR

#include "network_address_parameters.h"

#include <stdio.h>
#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>

namespace Supla {
namespace Html {

namespace {
constexpr const char* kModeSuffix = "mode";
constexpr const char* kStaticBoxSuffix = "static_box";
constexpr const char* kIpSuffix = "ip";
constexpr const char* kMaskSuffix = "mask";
constexpr const char* kGatewaySuffix = "gateway";
constexpr const char* kDns1Suffix = "dns1";
constexpr const char* kDns2Suffix = "dns2";
constexpr char kIpv4Pattern[] =
"^((25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])\\.){3}"
"(25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])$";
constexpr char kNetmaskPattern[] =
"^(((25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])\\.){3}"
"(25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])|/?([1-9]|[12][0-9]|3[0-2]))$";
constexpr char kScript[] =
"<script>"
"function showHideNetifStaticSettings(selectId, boxId){"
"var select=document.getElementById(selectId),"
"box=document.getElementById(boxId);"
"if(select==null||box==null){return;}"
"var staticMode=(select.value==\"1\");"
"box.style.display=staticMode?\"block\":\"none\";"
"var fields=box.getElementsByTagName(\"input\");"
"for(var i=0;i<fields.length;i++){"
"if(fields[i].getAttribute(\"data-static-required\")==\"1\"){"
"if(staticMode){fields[i].setAttribute(\"required\",\"required\");}"
"else{fields[i].removeAttribute(\"required\");}"
"}"
"}"
"}"
"</script>";
}  // namespace

NetworkAddressParameters::NetworkAddressParameters(const char* blobName,
                                                   const char* prefix)
    : blobName_(blobName), prefix_(prefix) {
}

void NetworkAddressParameters::ensureLoaded() {
  if (loaded_) {
    return;
  }
  normalizeDhcpNetifConfig(&config_);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && blobName_) {
    cfg->loadNetifConfig(blobName_, &config_);
  }
  loaded_ = true;
}

void NetworkAddressParameters::buildKey(char* out,
                                        size_t outSize,
                                        const char* suffix) const {
  if (out == nullptr || outSize == 0) {
    return;
  }
  snprintf(out, outSize, "%s_%s", prefix_ ? prefix_ : "", suffix ? suffix : "");
}

bool NetworkAddressParameters::matchKey(const char* key,
                                        const char* suffix) const {
  if (key == nullptr || suffix == nullptr) {
    return false;
  }
  char expected[64] = {};
  buildKey(expected, sizeof(expected), suffix);
  return strcmp(key, expected) == 0;
}

void NetworkAddressParameters::renderField(Supla::WebSender* sender,
                                           const char* suffix,
                                           const char* label,
                                           const char* value,
                                           const char* placeholder,
                                           const char* pattern,
                                           const char* title,
                                           bool requiredWhenStatic) {
  char key[64] = {};
  buildKey(key, sizeof(key), suffix);
  bool staticMode = config_.ipMode == static_cast<uint8_t>(NetifIpMode::Static);
  sender->labeledField(key, label, [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "text")
        .attr("name", key)
        .attr("id", key)
        .attr("maxlength", 15)
        .attr("inputmode", "decimal")
        .attr("placeholder", placeholder)
        .attr("pattern", pattern)
        .attr("title", title)
        .attrIf("required", requiredWhenStatic && staticMode);
    if (requiredWhenStatic) {
      input.attr("data-static-required", "1");
    }
    if (value != nullptr) {
      input.attr("value", value);
    }
    input.finish();
  });
}

void NetworkAddressParameters::send(Supla::WebSender* sender) {
  if (sender == nullptr || prefix_ == nullptr) {
    return;
  }

  ensureLoaded();

  char ipBuf[32] = {};
  char maskBuf[32] = {};
  char gatewayBuf[32] = {};
  char dns1Buf[32] = {};
  char dns2Buf[32] = {};
  if (config_.ipMode == static_cast<uint8_t>(NetifIpMode::Static)) {
    formatIpv4Address(config_.ip, ipBuf, sizeof(ipBuf));
    formatIpv4Address(config_.netmask, maskBuf, sizeof(maskBuf));
    formatIpv4Address(config_.gateway, gatewayBuf, sizeof(gatewayBuf));
    formatIpv4Address(config_.dns1, dns1Buf, sizeof(dns1Buf));
    if (config_.dns2 != 0) {
      formatIpv4Address(config_.dns2, dns2Buf, sizeof(dns2Buf));
    }
  }

  char modeKey[64] = {};
  char staticBoxId[64] = {};
  char onChange[128] = {};
  buildKey(modeKey, sizeof(modeKey), kModeSuffix);
  buildKey(staticBoxId, sizeof(staticBoxId), kStaticBoxSuffix);
  snprintf(onChange,
           sizeof(onChange),
           "showHideNetifStaticSettings(this.id, '%s')",
           staticBoxId);

  sender->labeledField(modeKey, "IP configuration", [&]() {
    auto select = sender->selectTag(modeKey, modeKey);
    select.attr("onchange", onChange).body([&]() {
      sender->selectOption(
          0,
          "DHCP (recommended)",
          config_.ipMode != static_cast<uint8_t>(NetifIpMode::Static));
      sender->selectOption(
          1,
          "Static",
          config_.ipMode == static_cast<uint8_t>(NetifIpMode::Static));
    });
  });

  sender->send(kScript);
  sender->toggleBox(
      staticBoxId,
      config_.ipMode == static_cast<uint8_t>(NetifIpMode::Static),
      [&]() {
        renderField(sender,
                    kIpSuffix,
                    "IP address",
                    ipBuf,
                    "192.168.1.100",
                    kIpv4Pattern,
                    "Enter an IPv4 address, e.g. 192.168.1.100",
                    true);
        renderField(sender,
                    kMaskSuffix,
                    "Subnet mask",
                    maskBuf,
                    "255.255.255.0 or /24",
                    kNetmaskPattern,
                    "Enter a subnet mask or prefix, e.g. 255.255.255.0 or /24",
                    true);
        renderField(sender,
                    kGatewaySuffix,
                    "Gateway",
                    gatewayBuf,
                    "192.168.1.1",
                    kIpv4Pattern,
                    "Enter an IPv4 gateway address, e.g. 192.168.1.1",
                    true);
        renderField(sender,
                    kDns1Suffix,
                    "DNS 1",
                    dns1Buf,
                    "8.8.8.8",
                    kIpv4Pattern,
                    "Enter an IPv4 DNS server address, e.g. 8.8.8.8",
                    true);
        renderField(sender,
                    kDns2Suffix,
                    "DNS 2 (optional)",
                    dns2Buf,
                    "8.8.4.4",
                    kIpv4Pattern,
                    "Enter an optional IPv4 DNS server address, e.g. 8.8.4.4",
                    false);
      });
}

bool NetworkAddressParameters::handleResponse(const char* key,
                                              const char* value) {
  ensureLoaded();
  if (key == nullptr) {
    return false;
  }

  char modeKey[64] = {};
  buildKey(modeKey, sizeof(modeKey), kModeSuffix);
  if (strcmp(key, modeKey) == 0) {
    responseSeen_ = true;
    config_.ipMode = (value && strcmp(value, "1") == 0)
                         ? static_cast<uint8_t>(NetifIpMode::Static)
                         : static_cast<uint8_t>(NetifIpMode::DHCP);
    return true;
  }

  if (matchKey(key, kIpSuffix)) {
    responseSeen_ = true;
    if (config_.ipMode == static_cast<uint8_t>(NetifIpMode::DHCP)) {
      config_.ip = 0;
      return true;
    }
    if (!parseIpv4Address(value, &config_.ip)) {
      invalidInput_ = true;
    }
    return true;
  }
  if (matchKey(key, kMaskSuffix)) {
    responseSeen_ = true;
    if (config_.ipMode == static_cast<uint8_t>(NetifIpMode::DHCP)) {
      config_.netmask = 0;
      return true;
    }
    if (!parseNetmaskOrPrefix(value, &config_.netmask)) {
      invalidInput_ = true;
    }
    return true;
  }
  if (matchKey(key, kGatewaySuffix)) {
    responseSeen_ = true;
    if (config_.ipMode == static_cast<uint8_t>(NetifIpMode::DHCP)) {
      config_.gateway = 0;
      return true;
    }
    if (!parseIpv4Address(value, &config_.gateway)) {
      invalidInput_ = true;
    }
    return true;
  }
  if (matchKey(key, kDns1Suffix)) {
    responseSeen_ = true;
    if (config_.ipMode == static_cast<uint8_t>(NetifIpMode::DHCP)) {
      config_.dns1 = 0;
      return true;
    }
    if (!parseIpv4Address(value, &config_.dns1)) {
      invalidInput_ = true;
    }
    return true;
  }
  if (matchKey(key, kDns2Suffix)) {
    responseSeen_ = true;
    if (config_.ipMode == static_cast<uint8_t>(NetifIpMode::DHCP)) {
      config_.dns2 = 0;
      return true;
    }
    if (value == nullptr || value[0] == '\0') {
      config_.dns2 = 0;
    } else if (!parseIpv4Address(value, &config_.dns2)) {
      invalidInput_ = true;
    }
    return true;
  }
  return false;
}

void NetworkAddressParameters::onProcessingEnd() {
  if (!responseSeen_) {
    return;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && !invalidInput_) {
    if (!cfg->saveNetifConfig(blobName_, config_)) {
      SUPLA_LOG_WARNING("Failed to save netif config \"%s\"",
                        blobName_ ? blobName_ : "");
    }
  } else if (invalidInput_) {
    SUPLA_LOG_WARNING("Rejected invalid netif config input \"%s\"",
                      blobName_ ? blobName_ : "");
  }

  loaded_ = false;
  responseSeen_ = false;
  invalidInput_ = false;
}

}  // namespace Html
}  // namespace Supla

#endif  // ARDUINO_ARCH_AVR
