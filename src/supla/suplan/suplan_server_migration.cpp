// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "suplan_server_migration.h"
#ifndef ARDUINO_ARCH_AVR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/crc16.h>
#include <supla/crypto.h>
#include <supla/control/action_trigger.h>
#include <supla/control/hvac_config.h>
#include <supla/control/valve_base.h>
#include <supla/sensor/container.h>
#include <supla/storage/config.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/suplan_peer_store.h>
#include <suplan/suplan_config.h>
#include <suplan/suplan_wire.h>
namespace Supla {
namespace Device {
bool validBindingRecord(const uint8_t *bytes, size_t size) {
  SupLan::PeerContext context;
  if (!bytes || size < 29 || bytes[0] != 1 ||
      size != 29U + 12U * bytes[28] ||
      !SupLan::decodePeerContext(bytes + 1, &context)) return false;
  for (uint16_t i = 0; i < bytes[28]; ++i) {
    const auto entry = bytes + 29 + 12 * i;
    const auto source = SupLan::getUint32(entry + 2);
    const auto target = SupLan::getUint32(entry + 7);
    if (entry[0] != 1 || entry[1] != SupLan::kResourceTypeChannel ||
        !source || source > INT32_MAX ||
        entry[6] != SupLan::kResourceTypeChannel ||
        !target || target > INT32_MAX || entry[11] != 0) return false;
    for (uint16_t j = 0; j < i; ++j)
      if (!memcmp(entry, bytes + 29 + 12 * j, 12)) return false;
  }
  return true;
}
namespace {
uint32_t word(const uint8_t *p) {
  return p[0] | (static_cast<uint32_t>(p[1]) << 8) |
      (static_cast<uint32_t>(p[2]) << 16) |
      (static_cast<uint32_t>(p[3]) << 24);
}
bool save(Config *cfg, const char *key, const void *data, int size) {
  // The fence remains durable on a failed/partial feature write. A later
  // commit may install this sanitized value, which is safe and replayable.
  return cfg->setBlob(key, static_cast<const char *>(data), size) &&
      cfg->commit();
}
// Short-lived feature-key snapshot: four families x 255 slots.
// Callbacks only inspect metadata; Config reads/writes happen after iteration.
struct Inventory {
  uint8_t present[4][32] = {};
  bool has(int family, int slot) const {
    return present[family][slot / 8] & (1u << (slot % 8));
  }
  static bool visit(const char *key, ConfigValueType type, void *context) {
    auto self = static_cast<Inventory *>(context);
    const char *number = key;
    if (*number < '0' || *number > '9') return true;
    const auto slot = strtol(number, nullptr, 10);
    if (slot < 0 || slot >= 255) return true;
    char expected[SUPLA_CONFIG_MAX_KEY_SIZE];
    for (int family = 0; family < 4; ++family) {
      // Four fixed formats only; no untrusted formatting input.
      switch (family) {
        case 0: snprintf(expected, sizeof(expected), "%d_hvac_cfg2",
                         static_cast<int>(slot)); break;
        case 1: snprintf(expected, sizeof(expected), "%d_cnt_cfg2",
                         static_cast<int>(slot)); break;
        case 2: snprintf(expected, sizeof(expected), "%d_valve_cfg2",
                         static_cast<int>(slot)); break;
        default: snprintf(expected, sizeof(expected), "%d_at_tmp",
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
    snprintf(key, sizeof(key), "%d_at_tmp", n);
    size = keys.has(3, n) ? cfg->getBlobSize(key) : -1;
    if (keys.has(3, n) && size < 0) return true;
    if (size >= 0) {
      Control::ActionTriggerStoredConfig record;
      if (size != sizeof(record) ||
          !cfg->getBlob(key, reinterpret_cast<char *>(&record), size) ||
          !record.valid() || record.sensor.reference().kind ==
              ChannelReferenceKind::SERVER_CHANNEL) return true;
    }
  }
  SupLanPeerStore store(cfg);
  // Inspection never repairs storage; unknown/corrupt data remains fenced.
  return !store.load(0, false) || store.hasServerState();
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
    snprintf(key, sizeof(key), "%d_at_tmp", number);
    size = keys.has(3, number) ? cfg->getBlobSize(key) : -1;
    if (keys.has(3, number) && size < 0) return false;
    if (size >= 0) {
      Control::ActionTriggerStoredConfig stored;
      if (size != sizeof(stored) ||
          !cfg->getBlob(key, reinterpret_cast<char *>(&stored), size) ||
          !stored.valid()) return false;
      auto next = stored;
      next.sensor.assign(translate(stored.sensor.reference()));
      if (memcmp(&stored, &next, sizeof(next)) &&
          !save(cfg, key, &next, sizeof(next))) return false;
    }
  }
  SupLanPeerStore store(cfg);
  return store.load() && store.migrate(*this);
}
}  // namespace Device
}  // namespace Supla
#endif
