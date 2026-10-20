// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_SUPLAN_SERVER_MIGRATION_H_
#define SRC_SUPLA_SUPLAN_SUPLAN_SERVER_MIGRATION_H_
#ifndef ARDUINO_ARCH_AVR
#include <stdint.h>
#include <supla-common/proto.h>
#include <supla/channels/channel_reference.h>
namespace Supla {
class Config;
namespace Device {
// Transient view of the durable old-map fence. No second retained directory.
struct ServerMigration {
  static const int kPrefix = 44;
  static const int kMaxSize = kPrefix + 6 * SUPLA_CHANNELMAXCOUNT + 2;
  const uint8_t *record;
  ChannelReference translate(ChannelReference reference) const;
  bool migrate(Config *config) const;
  static bool hasServerState(Config *config);
};
}  // namespace Device
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_SUPLAN_SUPLAN_SERVER_MIGRATION_H_
