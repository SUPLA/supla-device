// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_CONTROL_REMOTE_ACTION_H_
#define SRC_SUPLA_CONTROL_REMOTE_ACTION_H_
#include <stddef.h>
#include <stdint.h>
#include <supla-common/proto.h>
namespace Supla {
namespace Control {
inline uint32_t actionUint32(const uint8_t *bytes) {
  return static_cast<uint32_t>(bytes[0]) |
      (static_cast<uint32_t>(bytes[1]) << 8) |
      (static_cast<uint32_t>(bytes[2]) << 16) |
      (static_cast<uint32_t>(bytes[3]) << 24);
}
inline int16_t actionInt16(const uint8_t *bytes) {
  return static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) |
                              (static_cast<uint16_t>(bytes[1]) << 8));
}
inline bool actionReservedZero(const uint8_t *bytes, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (bytes[i]) return false;
  }
  return true;
}
inline uint8_t actionValueResult(int32_t result) {
  return result > 0 ? SUPLA_RESULTCODE_TRUE : result == 0
      ? SUPLA_RESULTCODE_NOT_ALLOWED : SUPLA_RESULTCODE_ACTION_UNSUPPORTED;
}
}  // namespace Control
}  // namespace Supla
#endif  // SRC_SUPLA_CONTROL_REMOTE_ACTION_H_
