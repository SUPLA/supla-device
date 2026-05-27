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

#ifndef SRC_SUPLA_NETWORK_NETIF_CONFIG_H_
#define SRC_SUPLA_NETWORK_NETIF_CONFIG_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace Supla {

enum class NetifIpMode : uint8_t {
  DHCP = 0,
  Static = 1,
};

constexpr uint8_t NETIF_CONFIG_BLOB_VERSION = 1;

#pragma pack(push, 1)
struct NetifConfigBlob {
  uint8_t version = NETIF_CONFIG_BLOB_VERSION;
  uint8_t ipMode = static_cast<uint8_t>(NetifIpMode::DHCP);
  uint8_t reserved[2] = {};
  uint32_t ip = 0;       // Stored in network byte order.
  uint32_t netmask = 0;  // Stored in network byte order.
  uint32_t gateway = 0;  // Stored in network byte order.
  uint32_t dns1 = 0;     // Stored in network byte order.
  uint32_t dns2 = 0;     // Stored in network byte order.
};
#pragma pack(pop)

static_assert(sizeof(NetifConfigBlob) == 24,
              "Unexpected NetifConfigBlob size");

inline bool parseUnsignedDecimal(const char* text,
                                 uint32_t* result,
                                 uint32_t maxValue) {
  if (text == nullptr || result == nullptr || text[0] == '\0') {
    return false;
  }

  uint32_t value = 0;
  for (const char* ptr = text; *ptr; ++ptr) {
    if (*ptr < '0' || *ptr > '9') {
      return false;
    }
    uint32_t digit = static_cast<uint32_t>(*ptr - '0');
    if (value > (UINT32_MAX - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
    if (value > maxValue) {
      return false;
    }
  }

  *result = value;
  return true;
}

inline bool parseUnsignedDecimalRange(const char* begin,
                                      const char* end,
                                      uint32_t* result,
                                      uint32_t maxValue) {
  if (begin == nullptr || end == nullptr || result == nullptr ||
      begin >= end) {
    return false;
  }

  uint32_t value = 0;
  for (const char* ptr = begin; ptr < end; ++ptr) {
    if (*ptr < '0' || *ptr > '9') {
      return false;
    }
    uint32_t digit = static_cast<uint32_t>(*ptr - '0');
    if (value > (UINT32_MAX - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
    if (value > maxValue) {
      return false;
    }
  }

  *result = value;
  return true;
}

inline bool parseIpv4Address(const char* text, uint32_t* result) {
  if (text == nullptr || result == nullptr) {
    return false;
  }

  uint32_t ip = 0;
  const char* ptr = text;
  for (int i = 0; i < 4; i++) {
    const char* end = (i < 3) ? strchr(ptr, '.') : ptr + strlen(ptr);
    if (end == nullptr) {
      return false;
    }
    uint32_t value = 0;
    if (!parseUnsignedDecimalRange(ptr, end, &value, 255)) {
      return false;
    }
    if (i < 3) {
      if (*end != '.') {
        return false;
      }
      ptr = end + 1;
      if (*ptr == '\0') {
        return false;
      }
    } else if (*end != '\0') {
      return false;
    }
    if (i == 0) {
      ip = value << 24;
    } else if (i == 1) {
      ip |= value << 16;
    } else if (i == 2) {
      ip |= value << 8;
    } else {
      ip |= value;
    }
  }
  *result = ip;
  return true;
}

inline bool formatIpv4Address(uint32_t ip, char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return false;
  }

  int size = snprintf(out,
                      outSize,
                      "%u.%u.%u.%u",
                      static_cast<unsigned int>((ip >> 24) & 0xFF),
                      static_cast<unsigned int>((ip >> 16) & 0xFF),
                      static_cast<unsigned int>((ip >> 8) & 0xFF),
                      static_cast<unsigned int>(ip & 0xFF));
  return size > 0 && static_cast<size_t>(size) < outSize;
}

inline bool isValidNetmask(uint32_t netmask) {
  if (netmask == 0) {
    return false;
  }

  uint32_t inverted = ~netmask;
  return (inverted & (inverted + 1)) == 0;
}

inline bool isValidIpv4Address(uint32_t ip) {
  uint8_t firstOctet = static_cast<uint8_t>((ip >> 24) & 0xFF);
  if (ip == 0 || ip == 0xFFFFFFFFu) {
    return false;
  }
  if (firstOctet == 127) {
    return false;
  }
  if (firstOctet >= 224) {
    return false;
  }
  return true;
}

inline bool isSameSubnet(uint32_t ip, uint32_t netmask, uint32_t gateway) {
  return (ip & netmask) == (gateway & netmask);
}

inline bool isUsableSubnetHostAddress(uint32_t ip, uint32_t netmask) {
  uint32_t hostMask = ~netmask;
  if (hostMask == 0) {
    return true;
  }

  uint32_t hostPart = ip & hostMask;
  return hostPart != 0 && hostPart != hostMask;
}

inline bool parseNetmaskOrPrefix(const char* text, uint32_t* result) {
  if (text == nullptr || result == nullptr) {
    return false;
  }

  if (text[0] == '/') {
    ++text;
  }

  if (strchr(text, '.') != nullptr) {
    if (!parseIpv4Address(text, result)) {
      return false;
    }
    return isValidNetmask(*result);
  }

  uint32_t prefix = 0;
  if (!parseUnsignedDecimal(text, &prefix, 32)) {
    return false;
  }
  if (prefix == 0 || prefix > 32) {
    return false;
  }

  if (prefix == 32) {
    *result = 0xFFFFFFFFu;
  } else {
    *result = 0xFFFFFFFFu << (32 - prefix);
  }
  return isValidNetmask(*result);
}

inline void normalizeDhcpNetifConfig(NetifConfigBlob* cfg) {
  if (cfg == nullptr) {
    return;
  }

  cfg->version = NETIF_CONFIG_BLOB_VERSION;
  cfg->ipMode = static_cast<uint8_t>(NetifIpMode::DHCP);
  cfg->reserved[0] = 0;
  cfg->reserved[1] = 0;
  cfg->ip = 0;
  cfg->netmask = 0;
  cfg->gateway = 0;
  cfg->dns1 = 0;
  cfg->dns2 = 0;
}

inline bool isValidStaticNetifConfig(const NetifConfigBlob& cfg) {
  if (cfg.version != NETIF_CONFIG_BLOB_VERSION) {
    return false;
  }
  if (cfg.ipMode != static_cast<uint8_t>(NetifIpMode::Static)) {
    return false;
  }

  if (!isValidIpv4Address(cfg.ip) || !isValidNetmask(cfg.netmask) ||
      !isUsableSubnetHostAddress(cfg.ip, cfg.netmask) ||
      !isValidIpv4Address(cfg.gateway) ||
      !isUsableSubnetHostAddress(cfg.gateway, cfg.netmask) ||
      !isSameSubnet(cfg.ip, cfg.netmask, cfg.gateway) ||
      cfg.ip == cfg.gateway ||
      !isValidIpv4Address(cfg.dns1)) {
    return false;
  }

  if (cfg.dns2 != 0 && !isValidIpv4Address(cfg.dns2)) {
    return false;
  }

  return true;
}

inline bool isValidNetifConfigBlob(const NetifConfigBlob& cfg) {
  if (cfg.version != NETIF_CONFIG_BLOB_VERSION) {
    return false;
  }
  if (cfg.ipMode == static_cast<uint8_t>(NetifIpMode::DHCP)) {
    return true;
  }
  if (cfg.ipMode == static_cast<uint8_t>(NetifIpMode::Static)) {
    return isValidStaticNetifConfig(cfg);
  }
  return false;
}

}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_NETIF_CONFIG_H_
