// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_SUPLANTESTS_PEER_STORE_TEST_HELPERS_H_
#define EXTRAS_TEST_SUPLANTESTS_PEER_STORE_TEST_HELPERS_H_
#include <supla/crc16.h>
#include <supla/storage/config.h>
#include <supla/suplan/suplan_peer_store.h>
#include <suplan/suplan_wire.h>

#include <cstdlib>
#include <cstring>
#include <vector>
// Test-only record selectors retain familiar labels, not backend keys. They
// inspect/edit TLVs in sl-peers, including deliberately corrupt test records.
namespace PeerStoreTest {
inline unsigned half(const uint8_t *p) {
  return p[0] | (p[1] << 8);
}
inline void half(uint8_t *p, unsigned n) {
  p[0] = n;
  p[1] = n >> 8;
}
inline bool binding(const char *selector) {
  return !strncmp(selector, "slb_", 4);
}
inline unsigned slot(const char *selector) {
  return strtoul(selector + (binding(selector) ? 4 : 8), nullptr, 10);
}
inline std::vector<uint8_t> blob(Supla::Config &cfg) {
  const int size = cfg.getBlobSize("sl-peers");
  std::vector<uint8_t> data(size >= 9 ? size : 9);
  if (size >= 9 &&
      cfg.getBlob("sl-peers", reinterpret_cast<char *>(data.data()), size))
    return data;
  data.assign(9, 0);
  data[0] = 1;
  half(data.data() + 3, 9);
  half(data.data() + 7, calculateCrc16(data.data(), 7));
  return data;
}
inline std::vector<uint8_t> record(Supla::Config &cfg, const char *selector) {
  const auto data = blob(cfg);
  for (size_t p = 7; p + 4 < data.size(); p += 3 + half(data.data() + p + 1)) {
    const unsigned length = half(data.data() + p + 1);
    if (!length || p + 3 + length > data.size() - 2) break;
    if ((data[p] == 3) == binding(selector) && data[p + 3] == slot(selector))
      return {data.begin() + p + 4, data.begin() + p + 3 + length};
  }
  return {};
}
inline int size(Supla::Config &cfg, const char *selector) {
  const auto value = record(cfg, selector);
  return value.empty() ? -1 : static_cast<int>(value.size());
}
inline bool read(Supla::Config &cfg,
                 const char *selector,
                 char *out,
                 size_t n) {
  const auto value = record(cfg, selector);
  if (value.size() != n) return false;
  memcpy(out, value.data(), n);
  return true;
}
inline bool write(Supla::Config &cfg,
                  const char *selector,
                  const char *bytes,
                  size_t n) {
  auto data = blob(cfg);
  for (size_t p = 7; p + 4 < data.size(); p += 3 + half(data.data() + p + 1)) {
    const unsigned length = half(data.data() + p + 1);
    if (!length || p + 3 + length > data.size() - 2) return false;
    if ((data[p] == 3) != binding(selector) || data[p + 3] != slot(selector))
      continue;
    data.erase(data.begin() + p, data.begin() + p + 3 + length);
    half(data.data() + 1, half(data.data() + 1) - 1);
    break;
  }
  if (n) {
    const auto p = data.size() - 2;
    data.resize(data.size() + 4 + n);
    data[p] = binding(selector) ? 3 : static_cast<uint8_t>(bytes[1]);
    half(data.data() + p + 1, n + 1);
    data[p + 3] = slot(selector);
    memcpy(data.data() + p + 4, bytes, n);
    half(data.data() + 1, half(data.data() + 1) + 1);
  }
  half(data.data() + 3, data.size());
  half(data.data() + 5, data.size() >> 16);
  half(data.data() + data.size() - 2,
       calculateCrc16(data.data(), data.size() - 2));
  return cfg.setBlob(
      "sl-peers", reinterpret_cast<char *>(data.data()), data.size());
}
inline bool erase(Supla::Config &cfg, const char *selector) {
  return write(cfg, selector, nullptr, 0);
}
}  // namespace PeerStoreTest
#endif  // EXTRAS_TEST_SUPLANTESTS_PEER_STORE_TEST_HELPERS_H_
