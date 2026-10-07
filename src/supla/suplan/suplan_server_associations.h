// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLAN_SUPLAN_SERVER_ASSOCIATIONS_H_
#define SRC_SUPLA_SUPLAN_SUPLAN_SERVER_ASSOCIATIONS_H_

#ifndef ARDUINO_ARCH_AVR
#include <supla-common/proto.h>
#include <suplan/suplan_runtime.h>

namespace Supla {
class Config;
namespace Device {
class ServerIdentity;

// Caller-owned bounded provisioning workspace. No feature intent/config copy.
class ServerAssociations {
 public:
  static const int kProfileEntries =
      SUPLAN_MAX_TOTAL_ACL_ENTRIES > SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES
          ? SUPLAN_MAX_TOTAL_ACL_ENTRIES
          : SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES;
  static const int kMaxEntries = kProfileEntries < SUPLA_SUPLAN_MAX_ACL_ENTRIES
                                     ? kProfileEntries
                                     : SUPLA_SUPLAN_MAX_ACL_ENTRIES;
  static const int kSourcePrefix = 34;
  static const int kDestinationPrefix = 66;
  static const int kMaxRecordSize = kDestinationPrefix + 6 * kMaxEntries + 2;
  ServerAssociations(SupLan::PeerTable *peers, SupLan::Runtime *runtime);
  ~ServerAssociations();
  ServerAssociations(const ServerAssociations &) = delete;
  ServerAssociations &operator=(const ServerAssociations &) = delete;
  void load(Config *config, ServerIdentity *identity);
  void identityChanged();
  TDS_SuplaSetSuplanSourceAssociationResult accept(
      const TSDS_SuplaSetSuplanSourceAssociation &request);
  TDS_SuplaSetSuplanDestinationAssociationResult accept(
      const TSDS_SuplaSetSuplanDestinationAssociation &request);
  ServerIdentity *identity() const { return identity_; }
  bool available() const { return config_ && identity_ && peers_ && runtime_; }

 private:
  uint8_t apply(const TSuplaSuplanPeerContext &context, uint32_t revision,
                const TSuplaSuplanAclEntry *entries, uint16_t count,
                bool destination, const uint8_t *key);
  uint8_t validate(const SupLan::PeerContext &context, uint32_t revision,
                   const TSuplaSuplanAclEntry *entries, uint16_t count,
                   bool destination) const;
  bool persist(uint8_t slot, const uint8_t *record, int size);
  bool readRecord(uint8_t slot, SupLan::PeerContext *context,
                  uint32_t *revision, uint8_t *count, bool *destination);
  static bool owns(void *self, const SupLan::PeerContext &context,
                   const SupLan::ResourceId &resource);
  void clear(uint8_t index);
  Config *config_ = nullptr;
  ServerIdentity *identity_ = nullptr;
  SupLan::PeerTable *peers_;
  SupLan::Runtime *runtime_;
  // 0xff means unmanaged/free; persistent slot numbers never escape this class.
  uint8_t slots_[SUPLAN_MAX_PERSISTENT_PEERS];
  bool durable_[SUPLAN_MAX_PERSISTENT_PEERS] = {};
  bool busy_ = false;
  uint8_t record_[kMaxRecordSize] = {};
  uint8_t previous_[kMaxRecordSize] = {};
  TSuplaSuplanAclEntry wireEntries_[kMaxEntries] = {};
  SupLan::AclEntry entries_[kMaxEntries] = {};
};
}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_SUPLAN_SUPLAN_SERVER_ASSOCIATIONS_H_
