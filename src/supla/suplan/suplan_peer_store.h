// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_SUPLAN_PEER_STORE_H_
#define SRC_SUPLA_SUPLAN_SUPLAN_PEER_STORE_H_
#ifndef ARDUINO_ARCH_AVR
#include <stddef.h>
#include <stdint.h>
#include <suplan/suplan_acl.h>
#include <suplan/suplan_config.h>
namespace Supla {
class Config;
namespace Device {
struct ServerMigration;
// Short-lived whole-store transaction. No retained blob or per-record Config
// key.
class SupLanPeerStore {
 public:
  enum Type : uint8_t { Source = 1, Destination = 2, Bindings = 3 };
  static constexpr const char *kKey = "sl-peers";
  static constexpr size_t kHeader = 7;
  // Includes stable slot bytes, existing record encodings and outer TLVs.
  static constexpr size_t kMaximum =
      9 + 105U * SUPLAN_MAX_PERSISTENT_PEERS +
      6U * (SUPLAN_MAX_TOTAL_ACL_ENTRIES + SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES) +
      12U * SUPLAN_MAX_RESOURCE_BINDINGS;
  explicit SupLanPeerStore(Config *config) : config_(config) {
  }
  ~SupLanPeerStore();
  SupLanPeerStore(const SupLanPeerStore &) = delete;
  SupLanPeerStore &operator=(const SupLanPeerStore &) = delete;
  // extra: bounded space for records this transaction will insert.
  // Inspection may reject invalid data without erasing or committing it.
  bool load(size_t extra = 0, bool discardInvalid = true);
  const uint8_t *get(Type type, uint8_t slot, size_t *size) const;
  bool replace(Type type, uint8_t slot, const uint8_t *record, size_t size);
  void discardAssociations(uint32_t localSource, uint32_t rootEpoch,
                           const SupLan::PeerContext &replaced);
  bool prune(const SupLan::PeerContext &context,
             const SupLan::AclEntry *expected,
             uint16_t count);
  bool migrate(const ServerMigration &migration);
  bool save();
  bool hasBindings() const;
  bool hasServerState() const;
  static bool valid(const uint8_t *data, size_t size);
  // Temporary compatibility cleanup for unreleased test-device records only.
  static bool cleanupLegacy(Config *config, bool *pending = nullptr);

 private:
  Config *config_;
  uint8_t *data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
  void seal();
  void remove(size_t offset);
};
}  // namespace Device
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_SUPLAN_SUPLAN_PEER_STORE_H_
