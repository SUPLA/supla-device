// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_WEB_HOST_H_
#define SRC_SUPLA_NETWORK_WEB_HOST_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace Supla {

// Bounded HTTP authority parser. IPv6 syntax is checked by the platform's
// address parser; no DNS lookup is involved in matching a Host.
struct WebHost {
  char name[254] = {};
  uint16_t port = 0;
  bool hasPort = false;
  bool ipv6 = false;
  bool ipv4 = false;
  uint8_t address[4] = {};

  bool parse(const char *value) {
    *this = WebHost{};
    if (value == nullptr) {
      return false;
    }
    const size_t length = strnlen(value, 262);
    if (length == 0 || length >= 262) {
      return false;
    }
    const char *begin = value;
    const char *end = nullptr;
    const char *portText = nullptr;
    if (value[0] == '[') {
      ipv6 = true;
      begin++;
      end = strchr(begin, ']');
      if (end == nullptr || end == begin) {
        return false;
      }
      if (end[1] != '\0') {
        if (end[1] != ':') {
          return false;
        }
        portText = end + 2;
      }
    } else {
      end = strchr(value, ':');
      if (end != nullptr) {
        portText = end + 1;
      } else {
        end = value + length;
      }
    }
    size_t size = end - begin;
    if (size == 0 || size >= sizeof(name)) {
      return false;
    }
    if (portText != nullptr) {
      hasPort = true;
      if (*portText == '\0') {
        return false;
      }
      uint32_t number = 0;
      for (const char *p = portText; *p; p++) {
        if (*p < '0' || *p > '9') {
          return false;
        }
        number = number * 10 + (*p - '0');
        if (number > 65535) {
          return false;
        }
      }
      if (number == 0) {
        return false;
      }
      port = number;
    }
    memcpy(name, begin, size);
    name[size] = '\0';
    if (ipv6) {
      for (size_t i = 0; i < size; i++) {
        const char c = name[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F') || c == ':' || c == '.')) {
          return false;
        }
      }
      return true;
    }
    // A single terminal root dot is valid for a DNS name.
    const bool rootDot = name[size - 1] == '.';
    if (rootDot) {
      name[--size] = '\0';
    }
    if (size == 0) {
      return false;
    }
    bool numeric = true;
    for (size_t i = 0; i < size; i++) {
      const char c = name[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.')) {
        return false;
      }
      if (c == '.' && (i == 0 || name[i - 1] == '.')) {
        return false;
      }
      if (c != '.' && (c < '0' || c > '9')) {
        numeric = false;
      }
    }
    if (!numeric) {
      return name[size - 1] != '.';
    }
    if (rootDot) {
      return false;
    }
    // Require four decimal octets, without octal/shortened representations.
    ipv4 = true;
    const char *p = name;
    for (int i = 0; i < 4; i++) {
      const char *start = p;
      unsigned int number = 0;
      while (*p >= '0' && *p <= '9') {
        number = number * 10 + (*p++ - '0');
        if (number > 255 || p - start > 3) {
          return false;
        }
      }
      if (p == start || (p - start > 1 && *start == '0')) {
        return false;
      }
      address[i] = number;
      if (i < 3) {
        if (*p++ != '.') {
          return false;
        }
      } else if (*p != '\0') {
        return false;
      }
    }
    return true;
  }

  bool matchesName(const char *hostname) const {
    if (ipv4 || ipv6 || hostname == nullptr || hostname[0] == '\0') {
      return false;
    }
    size_t i = 0;
    while (name[i] && hostname[i]) {
      if (lower(name[i]) != lower(hostname[i])) {
        return false;
      }
      i++;
    }
    if (hostname[i] != '\0') {
      return false;
    }
    const char *suffix = name + i;
    if (*suffix == '\0') {
      return true;
    }
    const char *local = ".local";
    while (*suffix && *local && lower(*suffix) == *local) {
      suffix++;
      local++;
    }
    return *suffix == '\0' && *local == '\0';
  }

 private:
  static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
  }
};

}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_WEB_HOST_H_
