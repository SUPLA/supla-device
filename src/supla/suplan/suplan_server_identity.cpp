// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR

#include <supla/suplan/suplan_server_identity.h>

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <supla/crypto.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/bootstrap.h>
#include <supla/suplan/suplan_server_migration.h>
#include <supla/element.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/channels/channel_handle.h>
#include <supla/channels/channel.h>
#include <supla/crc16.h>
#include <supla/device/register_device.h>
#include <supla/storage/config.h>
#include <supla/log_wrapper.h>

namespace {
const char kRootKey[] = "sl-root";
const char kIdentityKey[] = "sl-identity";
const char kMigrationKey[] = "sl-migration";
using Supla::Device::ServerMigration;
const int kRootSize = 39;
// v1 was already an explicit ChannelNumber/ChannelId record. v2/v3 preserve
// its layout and encode synchronized/pending-transition state respectively.
const uint8_t kIdentityReady = 2;
const uint8_t kIdentityTransition = 3;

void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; ++i) {
    p[i] = static_cast<uint8_t>(v >> (8 * i));
  }
}
uint32_t get32(const uint8_t *p) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) {
    v |= static_cast<uint32_t>(p[i]) << (8 * i);
  }
  return v;
}
void seal(uint8_t *data, int size) {
  uint16_t crc = calculateCrc16(data, size - 2);
  data[size - 2] = static_cast<uint8_t>(crc);
  data[size - 1] = static_cast<uint8_t>(crc >> 8);
}
bool valid(const uint8_t *data, int size) {
  return calculateCrc16(data, size - 2) ==
             (data[size - 2] | (static_cast<uint16_t>(data[size - 1]) << 8));
}
bool uniform(const uint8_t *data, int size, uint8_t value) {
  for (int i = 0; i < size; ++i) {
    if (data[i] != value) {
      return false;
    }
  }
  return true;
}
bool validIds(const TSD_SuplaDeviceIdentities &s) {
  if (s.DeviceId <= 0 || s.ChannelCount < 0 ||
      s.ChannelCount > SUPLA_CHANNELMAXCOUNT) {
    return false;
  }
  for (int i = 0; i < s.ChannelCount; ++i) {
    if (s.ChannelId[i] <= 0) {
      return false;
    }
    for (int j = 0; j < i; ++j) {
      if (s.ChannelId[i] == s.ChannelId[j]) {
        return false;
      }
    }
  }
  return true;
}
bool validMigration(const uint8_t *record, int size) {
  if (size < ServerMigration::kPrefix + 2 ||
      size > ServerMigration::kMaxSize || !valid(record, size) ||
      record[0] != 1 || record[1] > 2 || record[2] > 1 ||
      record[3] > SUPLA_CHANNELMAXCOUNT ||
      size != ServerMigration::kPrefix + 6 * record[3] + 2 ||
      get32(record + 4) > INT32_MAX || get32(record + 8) > INT32_MAX ||
      (record[3] && !get32(record + 4))) return false;
  for (int i = 0; i < record[3]; ++i) {
    const auto entry = record + ServerMigration::kPrefix + 6 * i;
    if (entry[0] >= SUPLA_CHANNELMAXCOUNT || entry[5] > 1 ||
        !get32(entry + 1) || get32(entry + 1) > INT32_MAX) return false;
    for (int j = 0; j < i; ++j) {
      const auto prior = record + ServerMigration::kPrefix + 6 * j;
      if (prior[0] == entry[0] || get32(prior + 1) == get32(entry + 1))
        return false;
    }
  }
  return true;
}
// Bounded, transient serialization storage; never a frame-sized stack array.
struct Buffer {
  explicit Buffer(int size) : data(static_cast<uint8_t *>(calloc(size, 1))) {}
  ~Buffer() { free(data); }
  void reset() { free(data); data = nullptr; }
  uint8_t *data;
};
}  // namespace

Supla::Device::ServerIdentity *Supla::Device::ServerIdentity::active = nullptr;

Supla::Device::ServerIdentity::ServerIdentity() {
  rootValid = identityValid = rootDurable = identityDurable = false;
  registrationPrepared = registrationValid = transition = syncComplete = false;
  identitiesAccepted = contextInvalidated = false;
  migrationPending = fullMigration = false;
  topologyReady = true;
}

Supla::Device::ServerIdentity::~ServerIdentity() {
  abandonRegistration();
  if (active == this) {
    active = nullptr;
  }
  memset(root, 0, sizeof(root));
}

bool Supla::Device::ServerIdentity::channelChanging(uint8_t number,
                                                     bool removing) {
  if (!active) {
    return true;
  }
  for (int i = 0; active->registrationPrepared &&
                  i < active->registrationCount; ++i) {
    if (active->registrationNumbers[i] == number) {
      active->contextInvalidated = true;
      active->identitiesAccepted = false;
      break;
    }
  }
  if (active->forgetChannel(number)) {
    return true;
  }
  if (!removing) {
    return false;
  }
  // A destructor cannot retain the removed object. Forget all associations
  // with a durable barrier rather than leave one eligible for number reuse.
  // Persistent configuration changes must preflight forgetChannel() and honor
  // failure; no durable lifecycle change is possible on an unwritable backend.
  active->identityValid = false;
  active->transition = true;
  active->syncComplete = false;
  uint8_t tombstone[kIdentityPrefix + 2] = {kIdentityTransition};
  put32(tombstone + 2, active->deviceId);
  seal(tombstone, sizeof(tombstone));
  return active->persist(kIdentityKey, tombstone, sizeof(tombstone));
}

void Supla::Device::ServerIdentity::factoryReset() {
  if (active) {
    // Config reset already erased records. Drop runtime credentials/context.
    auto owner = active;
    auto cfg = owner->config;
    owner->load(nullptr);
    owner->config = cfg;
    active = owner;
  }
}

bool Supla::Device::ServerIdentity::rotateRoot() {
  bool previousValid = rootValid;
  rootValid = false;
  if (!ensureRoot()) {
    rootValid = previousValid;
    return false;
  }
  abandonRegistration();
  syncComplete = false;
  return true;
}

bool Supla::Device::ServerIdentity::capable() const {
#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32) || defined(ESP_PLATFORM) || \
    defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266) || \
    defined(SUPLA_LINUX) || defined(SUPLA_TEST)
  return config != nullptr;
#else
  return false;
#endif
}

void Supla::Device::ServerIdentity::abandonRegistration() {
  free(registrationNumbers);
  registrationNumbers = nullptr;
  registrationValid = registrationPrepared = identitiesAccepted = false;
}

void Supla::Device::ServerIdentity::load(Config *cfg) {
  abandonRegistration();
  active = cfg ? this : nullptr;
  config = cfg;
  epoch = 0;
  memset(root, 0, sizeof(root));
  for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next()) {
    ch->serverChannelId = 0;
  }
  rootValid = identityValid = registrationValid = registrationPrepared = false;
  rootDurable = identityDurable = false;
  transition = syncComplete = false;
  migrationPending = fullMigration = false;
  if (!config) {
    return;
  }
  migrationPending = config->getBlobSize(kMigrationKey) >= 0;
  // Unknown/damaged fences remain fenced; never infer that migration finished.
  fullMigration = migrationPending;
  uint8_t rootRecord[kRootSize] = {};
  if (config->getBlobSize(kRootKey) == kRootSize &&
      config->getBlob(kRootKey, reinterpret_cast<char *>(rootRecord),
                      sizeof(rootRecord)) &&
      rootRecord[0] == 1 && valid(rootRecord, kRootSize)) {
    uint32_t storedEpoch = get32(rootRecord + 1);
    if (storedEpoch != 0 && storedEpoch != UINT32_MAX &&
        !uniform(rootRecord + 5, 32, 0) && !uniform(rootRecord + 5, 32, 0xFF)) {
      memcpy(root, rootRecord + 5, sizeof(root));
      epoch = storedEpoch;
      rootValid = rootDurable = true;
    }
  }
  int size = config->getBlobSize(kIdentityKey);
  if (size < kIdentityPrefix + 2 || size > kIdentityMaxSize) {
    return;
  }
  Buffer workspace(kIdentityMaxSize);
  auto record = workspace.data;
  if (!record) {
    return;
  }
  if (!config->getBlob(kIdentityKey, reinterpret_cast<char *>(record), size) ||
      (record[0] != 1 && record[0] != kIdentityReady &&
       record[0] != kIdentityTransition) ||
      !valid(record, size)) {
    return;
  }
  uint8_t count = record[1];
  int32_t storedDeviceId = static_cast<int32_t>(get32(record + 2));
  if (count > SUPLA_CHANNELMAXCOUNT || storedDeviceId <= 0 ||
      size != kIdentityPrefix + 5 * count + 2) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    const uint8_t *entry = record + kIdentityPrefix + 5 * i;
    uint32_t id = get32(entry + 1);
    if (entry[0] >= SUPLA_CHANNELMAXCOUNT || id == 0 || id > INT32_MAX) {
      return;
    }
    for (int j = 0; j < i; ++j) {
      const uint8_t *previous = record + kIdentityPrefix + 5 * j;
      if (entry[0] == previous[0] || id == get32(previous + 1)) {
        return;
      }
    }
  }
  // Validate the whole record before changing any Channel fields.
  for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next()) {
    ch->serverChannelId = 0;
  }
  bool missing = false;
  for (int i = 0; i < count; ++i) {
    const uint8_t *entry = record + kIdentityPrefix + 5 * i;
    auto ch = Supla::Channel::GetByChannelNumber(entry[0]);
    if (ch) {
      ch->serverChannelId = get32(entry + 1);
    } else {
      missing = true;
    }
  }
  deviceId = storedDeviceId;
  identityGeneration = Supla::Channel::identityGeneration();
  identityValid = true;
  // Existing Channels remain usable; removed associations need cleanup/sync.
  identityDurable = !missing;
  transition = missing || record[0] == kIdentityTransition || migrationPending;
  if (migrationPending && topologyReady) resumeMigration(false);
  SUPLA_LOG_INFO(
      "SupLAN SERVER identity loaded: device=%" PRId32
      ", channels=%u, rootEpoch=%" PRIu32,
      deviceId, static_cast<unsigned>(count), rootEpoch());
}

bool Supla::Device::ServerIdentity::registrationStarted() {
  abandonRegistration();
  syncComplete = false;
  int count = Supla::RegisterDevice::getChannelCount();
  if (count < 0 || count > SUPLA_CHANNELMAXCOUNT) {
    return false;
  }
  int n = 0;
  for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next(), ++n) {
    int number = ch->getChannelNumber();
    if (n >= count) {
      abandonRegistration();
      return false;
    }
    if (number < 0 || number >= SUPLA_CHANNELMAXCOUNT ||
        Supla::Channel::GetByChannelNumber(number) != ch) {
      return false;
    }
  }
  if (n != count) {
    return false;
  }
  registrationNumbers = static_cast<uint8_t *>(malloc(count ? count : 1));
  if (!registrationNumbers) {
    return false;
  }
  n = 0;
  for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next(), ++n) {
    registrationNumbers[n] = static_cast<uint8_t>(ch->getChannelNumber());
  }
  registrationCount = static_cast<uint8_t>(count);
  contextInvalidated = false;
  registrationPrepared = true;
  active = this;
  return true;
}
TDS_SuplaDeviceChannel_D *
Supla::Device::ServerIdentity::registrationChannel_D(int index) {
  if (!active || !active->registrationContextValid() || index < 0 ||
      index >= active->registrationCount) {
    return nullptr;
  }
  return Supla::RegisterDevice::getChannelByNumberPtr_D(
      active->registrationNumbers[index]);
}
TDS_SuplaDeviceChannel_E *
Supla::Device::ServerIdentity::registrationChannel_E(int index) {
  if (!active || !active->registrationContextValid() || index < 0 ||
      index >= active->registrationCount) {
    return nullptr;
  }
  return Supla::RegisterDevice::getChannelByNumberPtr_E(
      active->registrationNumbers[index]);
}
bool Supla::Device::ServerIdentity::registrationContextValid() const {
  return registrationPrepared && !contextInvalidated;
}
bool Supla::Device::ServerIdentity::registrationInvalidated() const {
  return registrationPrepared && !registrationContextValid();
}
void Supla::Device::ServerIdentity::registrationSucceeded() {
  registrationValid = registrationContextValid();
}
void Supla::Device::ServerIdentity::disconnected() {
  abandonRegistration();
  syncComplete = false;
  // An unfinished identity transition survives an ordinary reconnect.
}
void Supla::Device::ServerIdentity::syncDone() {
  if (registrationValid && registrationContextValid() &&
      identitiesAccepted &&
      identityDurable) {
    if (identityTransition()) {
      Buffer workspace(kIdentityMaxSize);
      auto record = workspace.data;
      int size = config ? config->getBlobSize(kIdentityKey) : -1;
      if (!record || size < kIdentityPrefix + 2 || size > kIdentityMaxSize ||
          !config->getBlob(kIdentityKey,
                           reinterpret_cast<char *>(record), size) ||
          !valid(record, size)) {
        return;
      }
      record[0] = kIdentityReady;
      seal(record, size);
      if (!persist(kIdentityKey, record, size)) {
        return;
      }
      identityDurable = true;
    }
    if (migrationPending) {
      // Erasure must be durable before SERVER can become READY. On failure,
      // restage the fence, so unrelated later commits cannot release it.
      Buffer fence(ServerMigration::kMaxSize);
      const int size = config->getBlobSize(kMigrationKey);
      if (!fence.data || size < ServerMigration::kPrefix + 2 ||
          size > ServerMigration::kMaxSize ||
          !config->getBlob(kMigrationKey,
              reinterpret_cast<char *>(fence.data), size)) return;
      if (!config->eraseKey(kMigrationKey) || !config->commit()) {
        config->setBlob(kMigrationKey,
            reinterpret_cast<const char *>(fence.data), size);
        return;
      }
      migrationPending = fullMigration = false;
    }
    syncComplete = true;
    transition = false;
    identityGeneration = Supla::Channel::identityGeneration();
  }
}
bool Supla::Device::ServerIdentity::identityTransition() const {
  return transition || (identityValid && identityGeneration !=
      Supla::Channel::identityGeneration());
}
bool Supla::Device::ServerIdentity::serverSyncComplete() const {
  return syncComplete && !identityTransition();
}

bool Supla::Device::ServerIdentity::persist(const char *key,
                                            const uint8_t *data, int size) {
  if (!config) {
    return false;
  }
  // Restore staged data on failure so an unrelated later commit cannot install
  // an unacknowledged update behind the still-valid runtime snapshot.
  const int capacity = !strcmp(key, kMigrationKey)
      ? ServerMigration::kMaxSize : kIdentityMaxSize;
  Buffer workspace(capacity);
  auto previous = workspace.data;
  if (!previous) {
    return false;
  }
  int previousSize = config->getBlobSize(key);
  bool saved = previousSize >= 0 && previousSize <= capacity;
  if (saved && !config->getBlob(key, reinterpret_cast<char *>(previous),
                              previousSize)) {
    return false;
  }
  if (config->setBlob(key, reinterpret_cast<const char *>(data), size) &&
      config->commit()) {
    return true;
  }
  // A failed write/commit may damage either record in a shared Config file.
  // Keep valid runtime state, but require confirmed persistence before OK.
  rootDurable = identityDurable = false;
  if (saved) {
    config->setBlob(key, reinterpret_cast<const char *>(previous),
                    previousSize);
  } else {
    config->eraseKey(key);
  }
  return false;
}

bool Supla::Device::ServerIdentity::ensureRoot() {
  if (rootValid && rootDurable) {
    return true;
  }
#ifdef ARDUINO_ARCH_AVR
  return false;
#else
  uint8_t record[kRootSize] = {1};
  uint32_t nextEpoch = epoch;
  if (rootValid) {
    // Repair persistence of the same root; storage failure never rotates it.
    put32(record + 1, epoch);
    memcpy(record + 5, root, sizeof(root));
  } else {
    if (!Supla::Crypto::fillRandom(record + 5, 32)) {
      return false;
    }
    if (uniform(record + 5, 32, 0) || uniform(record + 5, 32, 0xFF) ||
        (epoch && memcmp(record + 5, root, sizeof(root)) == 0)) {
      return false;
    }
    nextEpoch = 0;
    for (int i = 0; i < 8; ++i) {
      if (!Supla::Crypto::fillRandom(record + 1, 4)) {
        return false;
      }
      nextEpoch = get32(record + 1);
      if (nextEpoch != 0 && nextEpoch != UINT32_MAX && nextEpoch != epoch) {
        break;
      }
    }
    if (nextEpoch == 0 || nextEpoch == UINT32_MAX || nextEpoch == epoch) {
      return false;
    }
  }
  seal(record, sizeof(record));
  if (!persist(kRootKey, record, sizeof(record))) {
    return false;
  }
  memcpy(root, record + 5, sizeof(root));
  epoch = nextEpoch;
  rootValid = rootDurable = true;
  return true;
#endif
}

TDS_SuplaDeviceIdentitiesResult Supla::Device::ServerIdentity::accept(
    const TSD_SuplaDeviceIdentities &snapshot) {
  identitiesAccepted = false;
  TDS_SuplaDeviceIdentitiesResult result = {};
  result.Result = SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT;
  result.RootEpoch = rootEpoch();
  if (!registrationValid || !registrationContextValid() ||
      !validIds(snapshot) || snapshot.ChannelCount != registrationCount) {
    return result;
  }
  // Interpret positions using the captured transmitted ChannelNumber sequence.
  bool identical = identityValid && deviceId == snapshot.DeviceId;
  bool changed = identityValid && (deviceId != snapshot.DeviceId ||
                                   identityTransition());
  for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next()) {
    uint32_t newId = 0;
    for (int i = 0; i < registrationCount; ++i) {
      if (registrationNumbers[i] == ch->getChannelNumber()) {
        newId = static_cast<uint32_t>(snapshot.ChannelId[i]);
        break;
      }
    }
    if (newId != ch->serverChannelId) {
      identical = false;
      if (ch->serverChannelId) {
        changed = true;
      }
    }
  }
  result.Result = SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
  if (!ensureRoot()) {
    return result;
  }
  result.RootEpoch = rootEpoch();
  const bool orphaned = !identityValid && config &&
      (config->getBlobSize(kIdentityKey) >= 0 ||
       ServerMigration::hasServerState(config));
  const bool full = orphaned ||
      (identityValid && deviceId != snapshot.DeviceId);
  if (full || changed || migrationPending) {
    if (!beginMigration(full, &snapshot)) return result;
  }
  if (!identical || !identityDurable) {
    Buffer workspace(kIdentityMaxSize);
    auto record = workspace.data;
    if (!record) {
      return result;
    }
    record[0] = (transition || changed) ?
        kIdentityTransition : kIdentityReady;
    record[1] = registrationCount;
    put32(record + 2, snapshot.DeviceId);
    // Canonical persistence order avoids rewrites for reordered registration.
    int outputIndex = 0;
    for (int number = 0; number < SUPLA_CHANNELMAXCOUNT; ++number) {
      for (int i = 0; i < registrationCount; ++i) {
        if (registrationNumbers[i] == number) {
          uint8_t *entry = record + kIdentityPrefix + 5 * outputIndex++;
          entry[0] = static_cast<uint8_t>(number);
          put32(entry + 1, snapshot.ChannelId[i]);
          break;
        }
      }
    }
    int size = kIdentityPrefix + 5 * registrationCount + 2;
    seal(record, size);
    if (!persist(kIdentityKey, record, size)) {
      return result;
    }
    identityDurable = false;
    if (!registrationContextValid()) {
      return result;
    }
    if (identical) {
      identityDurable = true;
      identitiesAccepted = true;
      result.Result = SUPLA_SUPLAN_RESULT_OK;
      return result;
    }
    for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next()) {
      ch->serverChannelId = 0;
      for (int i = 0; i < registrationCount; ++i) {
        if (registrationNumbers[i] == ch->getChannelNumber()) {
          ch->serverChannelId = static_cast<uint32_t>(snapshot.ChannelId[i]);
          break;
        }
      }
    }
    deviceId = snapshot.DeviceId;
    identityValid = identityDurable = true;
    // Initial bootstrap has no previous relations to suspend.
    transition = transition || changed;
    if (transition) {
      syncComplete = false;
    }
    identityGeneration = Supla::Channel::identityGeneration();
  }
  identitiesAccepted = true;
  result.Result = SUPLA_SUPLAN_RESULT_OK;
  return result;
}

bool Supla::Device::ServerIdentity::forgetChannel(uint8_t number) {
  auto forgotten = Supla::Channel::GetByChannelNumber(number);
  if (!forgotten || !identityValid) {
    return !migrationPending || beginMigration(false, nullptr, number);
  }
  if (!forgotten->serverChannelId) {
    // Explicit local references also require lifecycle cleanup for a Channel
    // added since registration, before its number can be reused.
    return beginMigration(false, nullptr, number);
  }
  if (!beginMigration(false, nullptr, number)) return false;
  Buffer workspace(kIdentityMaxSize);
  auto record = workspace.data;
  if (!record) {
    return false;
  }
  record[0] = kIdentityTransition;
  put32(record + 2, deviceId);
  int count = 0;
  for (int n = 0; n < SUPLA_CHANNELMAXCOUNT; ++n) {
    auto ch = Supla::Channel::GetByChannelNumber(n);
    if (ch && ch != forgotten && ch->serverChannelId) {
      uint8_t *entry = record + kIdentityPrefix + 5 * count++;
      entry[0] = static_cast<uint8_t>(n);
      put32(entry + 1, ch->serverChannelId);
    }
  }
  record[1] = static_cast<uint8_t>(count);
  int size = kIdentityPrefix + 5 * count + 2;
  seal(record, size);
  if (!persist(kIdentityKey, record, size)) {
    return false;
  }
  forgotten->serverChannelId = 0;
  for (int i = 0; registrationPrepared && i < registrationCount; ++i) {
    if (registrationNumbers[i] == number) {
      contextInvalidated = true;
      break;
    }
  }
  identityDurable = true;
  transition = true;
  identitiesAccepted = false;
  syncComplete = false;
  return true;
}

bool Supla::Device::ServerIdentity::identityAvailable() const {
  return identityValid && (!fullMigration || !scopeFenced());
}
Supla::Device::ServerChannelResolution Supla::Device::ServerIdentity::resolve(
    uint32_t channelId) const {
  if (!channelId || channelId > INT32_MAX ||
      !identityAvailable() || identityTransition()) {
    return {ServerChannelLocation::kUnresolved, 0};
  }
  for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next()) {
    if (channelId && ch->serverChannelId == channelId) {
      return {ServerChannelLocation::kLocal,
              static_cast<uint8_t>(ch->getChannelNumber())};
    }
  }
  return {ServerChannelLocation::kRemote, 0};
}
bool Supla::Device::ServerIdentity::reverse(uint8_t channelNumber,
                                            uint32_t *channelId) const {
  auto ch = Supla::Channel::GetByChannelNumber(channelNumber);
  if (!channelId || !identityAvailable() || identityTransition() ||
      !ch || !ch->serverChannelId) {
    return false;
  }
  *channelId = ch->serverChannelId;
  return true;
}

void Supla::Device::ServerIdentity::notifyMigration() {
  ServerAssociations::scopeChanged(this);
  if (fullScopeFenced()) Supla::ChannelHandle::invalidateServerScope();
  for (auto layer = Supla::Protocol::ProtocolLayer::first(); layer;
       layer = layer->next()) layer->suplanIdentityChanged();
}

bool Supla::Device::ServerIdentity::configuredServerChanged(Config *cfg) {
  if (!cfg) return false;
  if (cfg->getBlobSize(kIdentityKey) < 0 &&
      cfg->getBlobSize(kMigrationKey) < 0) return true;
  if (active) {
    if (active->config != cfg) return false;
    return active->beginMigration(true);
  }
  // User provisioning can happen before normal boot identity restoration.
  ServerIdentity temporary;
  temporary.topologyReady = false;
  temporary.load(cfg);
  return temporary.beginMigration(true);
}

bool Supla::Device::ServerIdentity::configuredService(
    Config *cfg, const char *server) {
  if (!cfg || !server || strlen(server) >= SUPLA_SERVER_NAME_MAXSIZE)
    return false;
  bool scoped = cfg->getBlobSize(kIdentityKey) >= 0 ||
      cfg->getBlobSize(kMigrationKey) >= 0;
  for (auto layer = Supla::Protocol::ProtocolLayer::first(); !scoped && layer;
       layer = layer->next()) scoped = supLanRequested(layer->getSdc());
  if (!scoped) return true;  // Legacy devices need no extra persistence.
  char previous[SUPLA_SERVER_NAME_MAXSIZE] = {};
  const bool hadService = cfg->getString("sl-service", previous,
                                        sizeof(previous));
  if (hadService) {
    if (!strcmp(previous, server)) return true;
    if (!configuredServerChanged(cfg)) return false;
  }
  // Record explicit application/YAML configuration, never resolved endpoints.
  if (cfg->setString("sl-service", server) && cfg->commit()) return true;
  if (hadService) cfg->setString("sl-service", previous);
  else cfg->eraseKey("sl-service");
  return false;
}

bool Supla::Device::ServerIdentity::beginMigration(bool full,
    const TSD_SuplaDeviceIdentities *next, int removed) {
  if (!config) return false;
  bool existed = config->getBlobSize(kMigrationKey) >= 0;
  const bool oldPending = migrationPending;
  const bool oldFull = fullMigration;
  const bool oldTransition = transition;
  const bool oldSync = syncComplete;
  const bool oldAccepted = identitiesAccepted;
  migrationPending = true;
  fullMigration = fullMigration || full;
  identitiesAccepted = syncComplete = false;
  transition = true;
  // Full scope loss also invalidates borrowed views if scratch allocation or
  // the very first fence write fails. LOCAL authority remains independent.
  if (full) notifyMigration();
  Buffer workspace(ServerMigration::kMaxSize);
  if (!workspace.data) return false;
  auto fence = workspace.data;
  int size = config->getBlobSize(kMigrationKey);
  if (existed) {
    if (size < ServerMigration::kPrefix + 2 ||
        size > ServerMigration::kMaxSize ||
        !config->getBlob(kMigrationKey,
            reinterpret_cast<char *>(fence), size) ||
        !validMigration(fence, size)) return false;
    bool oldMapIsCurrent = identityValid &&
        get32(fence + 4) == static_cast<uint32_t>(deviceId);
    for (int i = 0; i < fence[3] && oldMapIsCurrent; ++i) {
      const auto entry = fence + ServerMigration::kPrefix + 6 * i;
      const auto ch = Supla::Channel::GetByChannelNumber(entry[0]);
      oldMapIsCurrent = ch && ch->getServerChannelId() == get32(entry + 1);
    }
    int currentCount = 0;
    for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next())
      if (ch->getServerChannelId()) ++currentCount;
    oldMapIsCurrent = oldMapIsCurrent && currentCount == fence[3];
    bool changesCurrent = next && next->DeviceId != deviceId;
    for (int i = 0; next && i < registrationCount; ++i) {
      const auto ch =
          Supla::Channel::GetByChannelNumber(registrationNumbers[i]);
      if (!ch || ch->getServerChannelId() !=
                     static_cast<uint32_t>(next->ChannelId[i]))
        changesCurrent = true;
    }
    if (fence[1] == 2 &&
        ((!next && full) ||
         (next && (!identityValid || (!oldMapIsCurrent && changesCurrent))))) {
      // A second ownership change after installation needs the CURRENT map,
      // rather than replaying provenance from an earlier completed normalize.
      memset(fence, 0, ServerMigration::kMaxSize);
      existed = false;
    } else if (removed < 0 && (!full || fence[2])) {
      const bool notify = fence[1] != 2 || full || changesCurrent;
      workspace.reset();
      if (notify) notifyMigration();
      return resumeMigration(true);
    } else if (full) {
      fence[2] = 1;
    }
  }
  if (!existed) {
    fence[0] = 1;
    fence[2] = full;
    put32(fence + 4, deviceId);
    put32(fence + 8, next ? next->DeviceId : 0);
    for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next()) {
      const int n = ch->getChannelNumber();
      if (n < 255 && n != removed)
        fence[12 + n / 8] |= 1u << (n % 8);
    }
    // Read the persisted OLD map, not incidental current registration order.
    Buffer previous(kIdentityMaxSize);
    const int oldSize = config->getBlobSize(kIdentityKey);
    if (!previous.data) return false;
    if (identityValid && oldSize >= kIdentityPrefix + 2 &&
        oldSize <= kIdentityMaxSize &&
        config->getBlob(kIdentityKey,
            reinterpret_cast<char *>(previous.data), oldSize) &&
        valid(previous.data, oldSize) &&
        oldSize == kIdentityPrefix + 5 * previous.data[1] + 2) {
      fence[3] = previous.data[1];
      for (int i = 0; i < fence[3]; ++i) {
        auto out = fence + ServerMigration::kPrefix + 6 * i;
        memcpy(out, previous.data + kIdentityPrefix + 5 * i, 5);
        auto ch = Supla::Channel::GetByChannelNumber(out[0]);
        bool survives = ch && ch->getServerChannelId() == get32(out + 1);
        if (!full && next) {
          bool same = false;
          for (int j = 0; j < registrationCount; ++j)
            if (registrationNumbers[j] == out[0] &&
                static_cast<uint32_t>(next->ChannelId[j]) == get32(out + 1))
              same = true;
          survives = survives && same;
        }
        out[5] = survives && out[0] != removed;
      }
    }
    size = ServerMigration::kPrefix + 6 * fence[3] + 2;
  }
  if (removed >= 0) {
    fence[12 + removed / 8] &= ~(1u << (removed % 8));
    for (int i = 0; i < fence[3]; ++i) {
      auto entry = fence + ServerMigration::kPrefix + 6 * i;
      if (entry[0] == removed) entry[5] = 0;
    }
  }
  fence[1] = topologyReady ? 1 : 0;
  seal(fence, size);
  if (!persist(kMigrationKey, fence, size)) {
    if (!full && !existed) {
      // The proposed local removal/renumbering was refused. Nothing migrated.
      migrationPending = oldPending;
      fullMigration = oldFull;
      transition = oldTransition;
      syncComplete = oldSync;
      identitiesAccepted = oldAccepted;
    } else {
      notifyMigration();
    }
    return false;
  }
  workspace.reset();
  notifyMigration();
  return topologyReady ? resumeMigration(true) : true;
}

bool Supla::Device::ServerIdentity::resumeMigration(bool reload) {
  Buffer workspace(ServerMigration::kMaxSize);
  auto fence = workspace.data;
  const int size = config->getBlobSize(kMigrationKey);
  if (!fence || size < ServerMigration::kPrefix + 2 ||
      size > ServerMigration::kMaxSize ||
      !config->getBlob(kMigrationKey,
            reinterpret_cast<char *>(fence), size) ||
      !validMigration(fence, size)) return false;
  fullMigration = fence[2];
  if (!topologyReady) return true;
  if (fence[1] == 0) {
    // Deferred explicit user intent was recorded before dynamic topology load.
    memset(fence + 12, 0, 32);
    for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next()) {
      const auto n = ch->getChannelNumber();
      if (n < 255) fence[12 + n / 8] |= 1u << (n % 8);
    }
    for (int i = 0; i < fence[3]; ++i) {
      auto entry = fence + ServerMigration::kPrefix + 6 * i;
      const auto ch = Supla::Channel::GetByChannelNumber(entry[0]);
      entry[5] = ch && ch->getServerChannelId() == get32(entry + 1);
    }
    fence[1] = 1;
    seal(fence, size);
    if (!persist(kMigrationKey, fence, size)) return false;
  }
  if (fence[1] == 1) {
    const ServerMigration migration = {fence};
    if (!migration.migrate(config)) return false;
    fence[1] = 2;
    seal(fence, size);
    if (!persist(kMigrationKey, fence, size)) return false;
  }
  if (reload) {
    for (auto element = Supla::Element::begin(); element;
         element = element->next()) element->onServerScopeMigration();
  }
  return true;
}

bool Supla::Device::ServerIdentity::configurationChannel(
    uint8_t number, uint32_t *id) const {
  auto ch = Supla::Channel::GetByChannelNumber(number);
  if (!id || !configurationReady() || !ch || !ch->getServerChannelId())
    return false;
  *id = ch->getServerChannelId();
  return true;
}

#endif  // !ARDUINO_ARCH_AVR
