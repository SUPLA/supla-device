// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "suplan_server_migration.h"
#ifndef ARDUINO_ARCH_AVR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/control/hvac_config.h>
#include <supla/control/valve_base.h>
#include <supla/sensor/container.h>
#include <supla/storage/config.h>
#include <suplan/suplan_config.h>
#include <suplan/suplan_wire.h>
namespace Supla {
namespace Device {
namespace {
uint32_t word(const uint8_t *p) {
  return p[0] | (static_cast<uint32_t>(p[1]) << 8) |
      (static_cast<uint32_t>(p[2]) << 16) |
      (static_cast<uint32_t>(p[3]) << 24);
}
bool save(Config *cfg, const char *key, const void *data, int size) {
  // The fence remains durable on a failed/partial feature write. A later
  // commit may install this sanitized value, which is safe and replayable.
  if (!cfg->setBlob(key, static_cast<const char *>(data), size) ||
      !cfg->commit()) return false;
  char confirmed[sizeof(Control::ValveStoredConfigV2)];
  return size <= static_cast<int>(sizeof(confirmed)) &&
      cfg->getBlobSize(key) == size && cfg->getBlob(key, confirmed, size) &&
      !memcmp(confirmed, data, size);
}
// Short-lived key snapshot: 5 record families x 255 slots, no retained index.
// Callbacks only inspect metadata; Config reads/writes happen after iteration.
struct Inventory {
  uint8_t present[5][32] = {};
  bool has(int family, int slot) const {
    return present[family][slot / 8] & (1u << (slot % 8));
  }
  static bool visit(const char *key, ConfigValueType type, void *context) {
    auto self = static_cast<Inventory *>(context);
    const char *number = key;
    if (!strncmp(key, "sl-peer-", 8)) number += 8;
    else if (!strncmp(key, "slb_", 4)) number += 4;
    if (*number < '0' || *number > '9') return true;
    const auto slot = strtol(number, nullptr, 10);
    if (slot < 0 || slot >= 255) return true;
    char expected[SUPLA_CONFIG_MAX_KEY_SIZE];
    for (int family = 0; family < 5; ++family) {
      // Five fixed formats only; no untrusted formatting input.
      switch (family) {
        case 0: snprintf(expected, sizeof(expected), "%d_hvac_cfg2",
                         static_cast<int>(slot)); break;
        case 1: snprintf(expected, sizeof(expected), "%d_cnt_cfg2",
                         static_cast<int>(slot)); break;
        case 2: snprintf(expected, sizeof(expected), "%d_valve_cfg2",
                         static_cast<int>(slot)); break;
        case 3: snprintf(expected, sizeof(expected), "sl-peer-%d",
                         static_cast<int>(slot)); break;
        default: snprintf(expected, sizeof(expected), "slb_%d",
                          static_cast<int>(slot)); break;
      }
      if (strcmp(key, expected)) continue;
      // A known name with another type is damaged storage, not absent state.
      if (type != ConfigValueType::BLOB) return false;
      self->present[family][slot / 8] |= 1u << (slot % 8);
      break;
    }
    return true;
  }
  bool load(Config *cfg) {
    return cfg && cfg->forEachKey("", visit, this) ==
        ConfigIterationResult::COMPLETED;
  }
};
}  // namespace
bool ServerMigration::hasServerState(Config *cfg) {
  Inventory keys;
  if (!keys.load(cfg)) return true;  // Unknown inventory must fail closed.
  char key[SUPLA_CONFIG_MAX_KEY_SIZE];
  for (int n = 0; n < 255; ++n) {
    snprintf(key, sizeof(key), "%d_hvac_cfg2", n);
    int size = keys.has(0, n) ? cfg->getBlobSize(key) : -1;
    if (keys.has(0, n) && size < 0) return true;
    if (size >= 0) {
      Control::HvacStoredConfigV2 record;
      if ((size != sizeof(record) &&
           size != offsetof(Control::HvacStoredConfigV2, serverNone)) ||
          !cfg->getBlob(key, reinterpret_cast<char *>(&record), size) ||
          record.referenceNamespace !=
              Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER ||
          record.serverNone.rootEpoch) return true;
    }
    snprintf(key, sizeof(key), "%d_cnt_cfg2", n);
    size = keys.has(1, n) ? cfg->getBlobSize(key) : -1;
    if (keys.has(1, n) && size < 0) return true;
    if (size >= 0) {
      Sensor::ContainerStoredConfigV2 record;
      if (size != sizeof(record) ||
          !cfg->getBlob(key, reinterpret_cast<char *>(&record), size))
        return true;
      for (const auto &slot : record.config.sensorData)
        if (slot.source.kind ==
            static_cast<uint8_t>(ChannelReferenceKind::SERVER_CHANNEL))
          return true;
    }
    snprintf(key, sizeof(key), "%d_valve_cfg2", n);
    size = keys.has(2, n) ? cfg->getBlobSize(key) : -1;
    if (keys.has(2, n) && size < 0) return true;
    if (size >= 0) {
      Control::ValveStoredConfigV2 record;
      if (size != sizeof(record) ||
          !cfg->getBlob(key, reinterpret_cast<char *>(&record), size))
        return true;
      for (const auto &slot : record.config.sensorData)
        if (slot.kind ==
            static_cast<uint8_t>(ChannelReferenceKind::SERVER_CHANNEL))
          return true;
    }
    snprintf(key, sizeof(key), "sl-peer-%d", n);
    if (keys.has(3, n)) return true;
    snprintf(key, sizeof(key), "slb_%d", n);
    if (keys.has(4, n)) return true;
  }
  return false;
}
ChannelReference ServerMigration::translate(ChannelReference ref) const {
  if (!ref.valid()) return {};
  if (ref.kind == ChannelReferenceKind::NONE) return ref;
  if (ref.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER) {
    const bool survives = ref.id < 255 &&
        (record[12 + ref.id / 8] & (1u << (ref.id % 8))) &&
        Channel::GetByChannelNumber(ref.id);
    return survives ? ref : ChannelReference();
  }
  for (int i = 0; i < record[3]; ++i) {
    const auto entry = record + kPrefix + 6 * i;
    if (word(entry + 1) != ref.id) continue;
    if (record[2]) {
      return entry[5] && Channel::GetByChannelNumber(entry[0])
          ? ChannelReference::local(entry[0]) : ChannelReference();
    }
    // Selective map/lifecycle change clears only the affected old own ID.
    return entry[5] ? ref : ChannelReference();
  }
  return record[2] ? ChannelReference() : ref;
}
bool ServerMigration::migrate(Config *cfg) const {
  Inventory keys;
  if (!keys.load(cfg)) return false;
  char key[SUPLA_CONFIG_MAX_KEY_SIZE];
  for (int number = 0; number < 255; ++number) {
    snprintf(key, sizeof(key), "%d_hvac_cfg2", number);
    int size = keys.has(0, number) ? cfg->getBlobSize(key) : -1;
    if (keys.has(0, number) && size < 0) return false;
    if (size >= 0) {
      Control::HvacStoredConfigV2 stored;
      Control::HvacConfiguration decoded;
      if ((size != sizeof(stored) &&
           size != offsetof(Control::HvacStoredConfigV2, serverNone)) ||
          !cfg->getBlob(key, reinterpret_cast<char *>(&stored), size) ||
          !Control::restoreHvacConfig(stored, &decoded)) return false;
      auto next = stored;
      for (int i = 0; i < 6; ++i) {
        auto ref = translate(decoded.reference(stored.references[i]));
        next.references[i] = {};
        if (record[2] || stored.referenceNamespace ==
                Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER) {
          if (ref.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER)
            next.references[i].local = {
                static_cast<uint8_t>(ref.id), 1, {0, 0}};
        } else {
          next.references[i].channelId = ref.id;
        }
      }
      if (record[2]) {
        next.referenceNamespace =
            Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER;
        next.serverNone = {};
      }
      if (memcmp(&stored, &next, sizeof(next)) &&
          !save(cfg, key, &next, sizeof(next))) return false;
    }
    snprintf(key, sizeof(key), "%d_cnt_cfg2", number);
    size = keys.has(1, number) ? cfg->getBlobSize(key) : -1;
    if (keys.has(1, number) && size < 0) return false;
    if (size >= 0) {
      Sensor::ContainerStoredConfigV2 stored;
      if (size != sizeof(stored) ||
          !cfg->getBlob(key, reinterpret_cast<char *>(&stored), size) ||
          stored.version != 2 || !stored.config.valid()) return false;
      auto next = stored;
      for (auto &slot : next.config.sensorData)
        slot.source.assign(translate(slot.source.reference()));
      if (memcmp(&stored, &next, sizeof(next)) &&
          !save(cfg, key, &next, sizeof(next))) return false;
    }
    snprintf(key, sizeof(key), "%d_valve_cfg2", number);
    size = keys.has(2, number) ? cfg->getBlobSize(key) : -1;
    if (keys.has(2, number) && size < 0) return false;
    if (size >= 0) {
      Control::ValveStoredConfigV2 stored;
      if (size != sizeof(stored) ||
          !cfg->getBlob(key, reinterpret_cast<char *>(&stored), size) ||
          stored.version != 2 || !stored.config.valid()) return false;
      auto next = stored;
      for (auto &slot : next.config.sensorData)
        slot.assign(translate(slot.reference()));
      if (memcmp(&stored, &next, sizeof(next)) &&
          !save(cfg, key, &next, sizeof(next))) return false;
    }
  }
  if (!record[2]) return true;
  // These slots belong exclusively to SERVER provisioning. ResourceBinding
  // records can also describe LOCAL peers; preserve those explicitly.
  bool erased = false;
  for (int slot = 0; slot < 255; ++slot) {
    snprintf(key, sizeof(key), "sl-peer-%d", slot);
    if (keys.has(3, slot)) {
      if (!cfg->eraseKey(key)) return false;
      erased = true;
    }
    snprintf(key, sizeof(key), "slb_%d", slot);
    if (!keys.has(4, slot)) continue;
    const int size = cfg->getBlobSize(key);
    if (size < 0) return false;
    SupLan::PeerContext context = {};
    // Config::getBlob requires the complete blob, not an arbitrary prefix.
    // Binding records are bounded by the compiled wire envelope.
    const int capacity = 29 + 12 * SUPLAN_MAX_BINDINGS_PER_PEER;
    auto body = static_cast<uint8_t *>(calloc(capacity, 1));
    if (!body) return false;
    bool local = false;
    if (size <= capacity && size >= 29 &&
        cfg->getBlob(key, reinterpret_cast<char *>(body), size)) {
      local = SupLan::decodePeerContext(body + 1, &context) &&
          context.authorityType == SupLan::kAuthorityLocal;
    }
    free(body);
    if (local) continue;
    if (!cfg->eraseKey(key)) return false;
    erased = true;
  }
  return !erased || cfg->commit();
}
}  // namespace Device
}  // namespace Supla
#endif
