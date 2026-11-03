// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "suplan_peer_store.h"
#ifndef ARDUINO_ARCH_AVR
#include <string.h>
#include <supla-common/proto.h>
#include <supla/crc16.h>
#include <supla/crypto.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/suplan/suplan_server_migration.h>
#include <supla/suplan/suplan_server_associations.h>
#include <suplan/suplan_wire.h>

#include <new>
namespace Supla {
namespace Device {
namespace {
using namespace SupLan;  // NOLINT(build/namespaces)
constexpr size_t kLegacyKeyCapacity = 33;
uint16_t half(const uint8_t *p) {
  return p[0] | (p[1] << 8);
}
void half(uint8_t *p, uint16_t n) {
  p[0] = n;
  p[1] = n >> 8;
}
bool relation(const PeerContext &a, const PeerContext &b) {
  return a.authorityType == b.authorityType && a.authorityId == b.authorityId &&
         a.source.nameSpace == b.source.nameSpace &&
         a.source.nodeId == b.source.nodeId &&
         a.destination.nameSpace == b.destination.nameSpace &&
         a.destination.nodeId == b.destination.nodeId;
}
bool exists(Config *cfg, const char *key, bool *present, bool *blob) {
  struct Find {
    const char *key;
    bool present;
    ConfigValueType type;
  };
  Find found{key, false, ConfigValueType::BLOB};
  const auto result = cfg->forEachKey(
      key,
      [](const char *name, ConfigValueType type, void *context) {
        auto f = static_cast<Find *>(context);
        if (strcmp(name, f->key)) return true;
        f->present = true;
        f->type = type;
        return false;
      },
      &found);
  *present = found.present;
  *blob = found.type == ConfigValueType::BLOB;
  return result == ConfigIterationResult::COMPLETED ||
         (result == ConfigIterationResult::STOPPED && found.present);
}
}  // namespace
SupLanPeerStore::~SupLanPeerStore() {
  if (data_) Crypto::secureClear(data_, capacity_);
  delete[] data_;
}
bool SupLanPeerStore::cleanupLegacy(Config *cfg, bool *pending) {
  if (!cfg) return false;
  // Delete outside enumeration callbacks (LittleFS may be mounted by iterator).
  bool removed = false;
  for (;;) {
    char key[kLegacyKeyCapacity] = {};
    const auto result = cfg->forEachKey(
        "",
        [](const char *name, ConfigValueType, void *context) {
          if (strncmp(name, "sl-peer-", 8) && strncmp(name, "slb_", 4))
            return true;
          if (strlen(name) >= kLegacyKeyCapacity) return false;
          strcpy(static_cast<char *>(context), name);  // NOLINT(runtime/printf)
          return false;
        },
        key);
    if (result != ConfigIterationResult::COMPLETED &&
        !(result == ConfigIterationResult::STOPPED && key[0]))
      return false;
    if (!key[0]) break;
    if (!cfg->eraseKey(key)) return false;
    removed = true;
    if (pending) *pending = true;
  }
  // Also complete a previous interrupted cleanup's staged erases.
  if (pending) *pending = *pending || removed;
  const bool cleaned = removed || (pending && *pending);
  if (cleaned && !cfg->commit()) return false;
  if (pending) *pending = false;
  if (cleaned) SUPLA_LOG_WARNING("SupLAN discarded obsolete peer/binding keys");
  return true;
}
void SupLanPeerStore::seal() {
  data_[0] = 1;
  half(data_ + 3, size_);
  half(data_ + 5, size_ >> 16);
  half(data_ + size_ - 2, calculateCrc16(data_, size_ - 2));
}
bool SupLanPeerStore::load(size_t extra, bool discardInvalid) {
  if (data_ || !config_ || extra > kMaximum) return false;
  bool present = false, blob = false;
  if (!exists(config_, kKey, &present, &blob)) return false;
  const int length = present ? config_->getBlobSize(kKey) : 0;
  if (length < 0 && blob) return false;
  if (present &&
      (!blob || length < 9 || static_cast<size_t>(length) > kMaximum)) {
    if (!discardInvalid || !config_->eraseKey(kKey) || !config_->commit())
      return false;
    SUPLA_LOG_WARNING("SupLAN discarded invalid peer store (size)");
    ServerAssociations::storageChanged(config_, false);
    present = false;
  }
  size_ = present ? length : 9;
  // Reserve only the existing data plus the incoming record, in one allocation.
  capacity_ = extra > kMaximum - size_ ? kMaximum : size_ + extra;
  data_ = new (std::nothrow) uint8_t[capacity_]();
  if (!data_) return false;
  if (present &&
      !config_->getBlob(kKey, reinterpret_cast<char *>(data_), size_))
    return false;
  if (present && !valid(data_, size_)) {
    if (!discardInvalid || !config_->eraseKey(kKey) || !config_->commit())
      return false;
    SUPLA_LOG_WARNING("SupLAN discarded invalid peer store (format/CRC)");
    ServerAssociations::storageChanged(config_, false);
    size_ = 9;
    memset(data_, 0, size_);
  }
  seal();
  return true;
}
const uint8_t *SupLanPeerStore::get(Type type,
                                    uint8_t slot,
                                    size_t *length) const {
  if (!data_) return nullptr;
  for (size_t p = kHeader; p < size_ - 2; p += 3 + half(data_ + p + 1)) {
    if ((data_[p] == type || (type == Source && data_[p] == Destination)) &&
        data_[p + 3] == slot) {
      *length = half(data_ + p + 1) - 1;
      return data_ + p + 4;
    }
  }
  return nullptr;
}
void SupLanPeerStore::remove(size_t offset) {
  const auto length = 3 + half(data_ + offset + 1);
  memmove(data_ + offset, data_ + offset + length, size_ - offset - length);
  size_ -= length;
  half(data_ + 1, half(data_ + 1) - 1);
}
bool SupLanPeerStore::replace(Type type,
                              uint8_t slot,
                              const uint8_t *record,
                              size_t length) {
  if (!data_ || slot >= SUPLAN_MAX_PERSISTENT_PEERS || length > UINT16_MAX - 1)
    return false;
  size_t previous = 0;
  const auto old = get(type == Bindings ? Bindings : Source, slot, &previous);
  const size_t offset = old ? old - data_ - 4 : size_ - 2;
  if (old) previous += 4;
  const size_t required = size_ - previous + (length ? length + 4 : 0);
  if (required > capacity_ || (length && !record)) return false;
  if (previous) remove(offset);
  if (length) {
    auto out = data_ + size_ - 2;
    out[0] = type;
    half(out + 1, length + 1);
    out[3] = slot;
    memcpy(out + 4, record, length);
    size_ += length + 4;
    half(data_ + 1, half(data_ + 1) + 1);
  }
  seal();
  return true;
}
bool SupLanPeerStore::save() {
  seal();
  if (!valid(data_, size_) ||
      !config_->setBlob(kKey, reinterpret_cast<const char *>(data_), size_) ||
      !config_->commit()) return false;
  ServerAssociations::storageChanged(config_, hasBindings());
  return true;
}
bool SupLanPeerStore::hasBindings() const {
  for (size_t p = kHeader; p < size_ - 2; p += 3 + half(data_ + p + 1))
    if (data_[p] == Bindings) return true;
  return false;
}
bool SupLanPeerStore::hasServerState() const {
  for (size_t p = kHeader; p < size_ - 2; p += 3 + half(data_ + p + 1)) {
    if (data_[p] != Bindings || data_[p + 5] == SupLan::kAuthorityServer)
      return true;
  }
  return false;
}
bool SupLanPeerStore::valid(const uint8_t *bytes, size_t length) {
  using namespace SupLan;  // NOLINT(build/namespaces)
  if (!bytes || length < 9 || length > kMaximum || bytes[0] != 1 ||
      (half(bytes + 3) | (static_cast<uint32_t>(half(bytes + 5)) << 16)) !=
          length ||
      half(bytes + length - 2) != calculateCrc16(bytes, length - 2))
    return false;
  uint16_t records = 0, peers = 0, sets = 0;
  uint32_t acl = 0, expected = 0, bindings = 0;
  size_t p = kHeader;
  for (; p < length - 2; ++records) {
    if (length - 2 - p < 4) return false;
    const uint8_t type = bytes[p], slot = bytes[p + 3];
    const size_t size = half(bytes + p + 1);
    if (!size || size > length - 2 - p - 3 ||
        slot >= SUPLAN_MAX_PERSISTENT_PEERS || type < Source || type > Bindings)
      return false;
    const auto record = bytes + p + 4;
    const size_t body = size - 1;
    PeerContext context;
    const auto encoded = record + (type == Bindings ? 1 : 2);
    if (body < (type == Bindings ? 29U : 36U) ||
        !decodePeerContext(encoded, &context))
      return false;
    for (size_t q = kHeader; q < p; q += 3 + half(bytes + q + 1)) {
      const bool otherBinding = bytes[q] == Bindings;
      if (otherBinding != (type == Bindings)) continue;
      if (bytes[q + 3] == slot ||
          !memcmp(encoded, bytes + q + 4 + (otherBinding ? 1 : 2), 27))
        return false;
      // Association generations of the same ordered relation cannot coexist.
      if (type != Bindings) {
        PeerContext other;
        decodePeerContext(bytes + q + 6, &other);
        if (relation(context, other)) return false;
      }
    }
    if (type == Bindings) {
      if (!validBindingRecord(record, body) || !record[28] ||
          record[28] > SUPLAN_MAX_BINDINGS_PER_PEER)
        return false;
      bindings += record[28];
      ++sets;
    } else {
      const auto count = record[33];
      const size_t prefix = type == Source ? 34 : 66;
      if (record[0] != 1 || record[1] != type ||
          context.authorityType != kAuthorityServer || context.authorityId ||
          context.source.nameSpace != kNodeIdDevice ||
          context.destination.nameSpace != kNodeIdDevice ||
          !context.source.nodeId || !context.destination.nodeId ||
          context.source.nodeId == context.destination.nodeId ||
          !context.rootEpoch || context.rootEpoch == UINT32_MAX ||
          !context.peerGeneration || context.source.nodeId > INT32_MAX ||
          context.destination.nodeId > INT32_MAX || !getUint32(record + 29) ||
          (type == Destination && !count) ||
          count > SUPLA_SUPLAN_MAX_ACL_ENTRIES ||
          body != prefix + 6U * count + 2 ||
          half(record + body - 2) != calculateCrc16(record, body - 2))
        return false;
      if (type == Destination) {
        bool zero = true, ones = true;
        for (int i = 0; i < 32; ++i) {
          zero = zero && record[34 + i] == 0;
          ones = ones && record[34 + i] == 255;
        }
        if (zero || ones) return false;
      }
      for (unsigned i = 0; i < count; ++i) {
        const auto entry = record + prefix + 6 * i;
        const uint32_t id = getUint32(entry + 1);
        if ((entry[0] != kResourceTypeChannel &&
             entry[0] != kResourceTypeDevice) ||
            !id || id > INT32_MAX || !entry[5] || (entry[5] & ~7) ||
            ((entry[5] & kPermissionRead) && (entry[5] & kPermissionControl)) ||
            (entry[0] == kResourceTypeDevice && id != context.source.nodeId))
          return false;
        if (i && (entry[-6] > entry[0] ||
                  (entry[-6] == entry[0] && getUint32(entry - 5) >= id)))
          return false;
      }
      if (type == Source)
        acl += count;
      else
        expected += count;
      ++peers;
    }
    p += 3 + size;
  }
  return p == length - 2 && records == half(bytes + 1) &&
         peers <= SUPLAN_MAX_PERSISTENT_PEERS &&
         sets <= SUPLAN_MAX_PERSISTENT_PEERS &&
         acl <= SUPLAN_MAX_TOTAL_ACL_ENTRIES &&
         expected <= SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES &&
         bindings <= SUPLAN_MAX_RESOURCE_BINDINGS;
}
void SupLanPeerStore::discardAssociations(
    uint32_t localSource, uint32_t rootEpoch,
    const SupLan::PeerContext &replaced) {
  for (size_t p = kHeader; p < size_ - 2;) {
    SupLan::PeerContext context;
    if (data_[p] != Bindings &&
        SupLan::decodePeerContext(data_ + p + 6, &context) &&
        (relation(context, replaced) ||
         (data_[p] == Source && context.source.nodeId == localSource &&
          context.rootEpoch != rootEpoch))) {
      remove(p);
    } else {
      p += 3 + half(data_ + p + 1);
    }
  }
}
bool SupLanPeerStore::prune(const SupLan::PeerContext &context,
                            const SupLan::AclEntry *expected,
                            uint16_t count) {
  using namespace SupLan;  // NOLINT(build/namespaces)
  uint8_t encoded[27];
  encodePeerContext(&context, encoded);
  for (size_t p = kHeader; p < size_ - 2;) {
    auto body = data_ + p + 4;
    PeerContext old;
    if (data_[p] != Bindings || !decodePeerContext(body + 1, &old) ||
        !relation(old, context)) {
      p += 3 + half(data_ + p + 1);
      continue;
    }
    unsigned output = 0;
    const auto before = body[28];
    for (unsigned i = 0; i < before; ++i) {
      const auto edge = body + 29 + 12 * i;
      bool allowed = false;
      if (!memcmp(encoded, body + 1, 27)) {
        for (unsigned j = 0; j < count; ++j) {
          const auto &entry = expected[j];
          allowed =
              allowed ||
              ((entry.permissions & (kPermissionRead | kPermissionControl)) &&
               ((entry.resource.type == kResourceTypeChannel &&
                 entry.resource.id == getUint32(edge + 2)) ||
                (entry.resource.type == kResourceTypeDevice &&
                 entry.resource.id == context.source.nodeId)));
        }
      }
      if (allowed) memmove(body + 29 + 12 * output++, edge, 12);
    }
    if (!output) {
      remove(p);
      continue;
    }
    const auto removed = 12 * (before - output);
    body[28] = output;
    const size_t end = p + 3 + half(data_ + p + 1);
    memmove(data_ + end - removed, data_ + end, size_ - end);
    size_ -= removed;
    half(data_ + p + 1, half(data_ + p + 1) - removed);
    p += 3 + half(data_ + p + 1);
  }
  seal();
  return true;
}
bool SupLanPeerStore::migrate(const ServerMigration &migration) {
  using namespace SupLan;  // NOLINT(build/namespaces)
  bool changed = false;
  for (size_t p = kHeader; p < size_ - 2;) {
    const auto type = data_[p];
    auto record = data_ + p + 4;
    PeerContext context;
    decodePeerContext(record + (type == Bindings ? 1 : 2), &context);
    if (migration.record[2] && context.authorityType == kAuthorityServer) {
      remove(p);
      changed = true;
      continue;
    }
    if (type == Destination ||
        (context.authorityType == kAuthorityLocal && migration.record[2])) {
      p += 3 + half(data_ + p + 1);
      continue;
    }
    const unsigned prefix = type == Bindings ? 29 : 34;
    const unsigned stride = type == Bindings ? 12 : 6;
    const unsigned countOffset = type == Bindings ? 28 : 33;
    const unsigned count = record[countOffset];
    unsigned output = 0;
    for (unsigned i = 0; i < count; ++i) {
      const auto entry = record + prefix + stride * i;
      const bool removed =
          type == Bindings
              ? migration
                        .translate(
                            ChannelReference::server(getUint32(entry + 7)))
                        .kind == ChannelReferenceKind::NONE
              : entry[0] == kResourceTypeChannel &&
                    migration
                            .translate(
                                ChannelReference::server(getUint32(entry + 1)))
                            .kind == ChannelReferenceKind::NONE;
      if (!removed) memmove(record + prefix + stride * output++, entry, stride);
    }
    if (!output && type == Bindings) {
      remove(p);
      changed = true;
      continue;
    }
    record[countOffset] = output;
    const unsigned removed = stride * (count - output);
    changed = changed || removed != 0;
    const size_t end = p + 3 + half(data_ + p + 1);
    if (type != Bindings)
      half(record + prefix + stride * output,
           calculateCrc16(record, prefix + stride * output));
    memmove(data_ + end - removed, data_ + end, size_ - end);
    size_ -= removed;
    half(data_ + p + 1, half(data_ + p + 1) - removed);
    p += 3 + half(data_ + p + 1);
  }
  seal();
  return !changed || save();
}
}  // namespace Device
}  // namespace Supla
#endif
