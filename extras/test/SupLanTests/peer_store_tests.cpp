// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/storage/key_value.h>
#include <supla/suplan/suplan_peer_store.h>
#include <supla/suplan/suplan_server_migration.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "peer_store_test_helpers.h"
namespace {
using Supla::Device::SupLanPeerStore;
class StoreConfig : public Supla::KeyValue {
 public:
  using KeyValue::getBlobSize;
  bool init() override {
    return true;
  }
  bool setBlob(const char *key, const char *value, size_t size) override {
    return !failWrite && KeyValue::setBlob(key, value, size);
  }
  bool getBlob(const char *key, char *value, size_t size) override {
    ++reads;
    return !failRead && KeyValue::getBlob(key, value, size);
  }
  bool eraseKey(const char *key) override {
    ++erases;
    return !failErase && KeyValue::eraseKey(key);
  }
  bool commit() override {
    ++commits;
    if (failCommit) return false;
    std::array<uint8_t, 65536> buffer = {};
    const auto n = serializeToMemory(buffer.data(), buffer.size());
    if (n == SIZE_MAX) return false;
    disk.assign(buffer.begin(), buffer.begin() + n);
    return true;
  }
  void reboot() {
    removeAllMemory();
    ASSERT_TRUE(initFromMemory(disk.data(), disk.size()));
  }
  bool failWrite = false, failRead = false, failCommit = false,
       failErase = false;
  int reads = 0, erases = 0, commits = 0;
  std::vector<uint8_t> disk;
};
const uint8_t kSource[] = {
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x65, 0x01, 0x00, 0x00, 0x00, 0xca, 0x00,
    0x00, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x01, 0xf5, 0x01, 0x50, 0x76,
};
const uint8_t kDestination[] = {
    0x01, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0xca, 0x01, 0x00, 0x00, 0x00, 0x65, 0x00,
    0x00, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x01, 0x00, 0x00, 0x02, 0x59, 0x01, 0xcd, 0xeb,
};
const uint8_t kLocalBinding[] = {
    0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4d, 0x03,
    0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x7b, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00,
};
const uint8_t kBlob[] = {
    0x01, 0x03, 0x00, 0xb2, 0x00, 0x00, 0x00, 0x01, 0x2b, 0x00, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x65, 0x01, 0x00, 0x00, 0x00, 0xca, 0x00, 0x00, 0x00, 0x7b,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00,
    0x01, 0xf5, 0x01, 0x50, 0x76, 0x02, 0x4b, 0x00, 0x01, 0x01, 0x02, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0xca, 0x01, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x00, 0x7b, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x01, 0x00, 0x00, 0x02, 0x59, 0x01, 0xcd, 0xeb, 0x03,
    0x2a, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x4d, 0x03, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0xfa, 0xb8,
};

TEST(PeerStore, FixedByteVectorEncodesAndDecodesAllRecordTypes) {
  StoreConfig cfg;
  ASSERT_TRUE(SupLanPeerStore::valid(kBlob, sizeof(kBlob)));
  {
    SupLanPeerStore store(&cfg);
    ASSERT_TRUE(store.load(sizeof(kSource) + sizeof(kDestination) +
                           sizeof(kLocalBinding) + 12));
    ASSERT_TRUE(
        store.replace(SupLanPeerStore::Source, 0, kSource, sizeof(kSource)));
    ASSERT_TRUE(store.replace(
        SupLanPeerStore::Destination, 1, kDestination, sizeof(kDestination)));
    ASSERT_TRUE(store.replace(
        SupLanPeerStore::Bindings, 0, kLocalBinding, sizeof(kLocalBinding)));
    ASSERT_TRUE(store.save());
  }
  std::array<uint8_t, sizeof(kBlob)> actual;
  ASSERT_TRUE(cfg.getBlob(
      "sl-peers", reinterpret_cast<char *>(actual.data()), actual.size()));
  EXPECT_EQ(memcmp(actual.data(), kBlob, actual.size()), 0);
  cfg.reboot();
  SupLanPeerStore store(&cfg);
  ASSERT_TRUE(store.load());
  size_t size = 0;
  const auto binding = store.get(SupLanPeerStore::Bindings, 0, &size);
  ASSERT_NE(binding, nullptr);
  ASSERT_EQ(size, sizeof(kLocalBinding));
  EXPECT_EQ(memcmp(binding, kLocalBinding, size), 0);
  EXPECT_TRUE(store.hasBindings());
  EXPECT_TRUE(store.hasServerState());
}
TEST(PeerStore, ReplacementKeepsOtherRecordsAndRejectsOversizeWithoutMutation) {
  StoreConfig cfg;
  SupLanPeerStore store(&cfg);
  ASSERT_TRUE(store.load(sizeof(kSource) + sizeof(kLocalBinding) + 8));
  ASSERT_TRUE(store.replace(SupLanPeerStore::Source, 0,
                            kSource, sizeof(kSource)));
  ASSERT_TRUE(store.replace(SupLanPeerStore::Bindings, 0,
                            kLocalBinding, sizeof(kLocalBinding)));
  ASSERT_FALSE(store.replace(SupLanPeerStore::Source, 0, kSource,
                             SupLanPeerStore::kMaximum));
  size_t size = 0;
  auto source = store.get(SupLanPeerStore::Source, 0, &size);
  ASSERT_NE(source, nullptr);
  ASSERT_EQ(size, sizeof(kSource));
  EXPECT_EQ(memcmp(source, kSource, size), 0);
  ASSERT_TRUE(store.replace(SupLanPeerStore::Source, 0, nullptr, 0));
  const auto binding = store.get(SupLanPeerStore::Bindings, 0, &size);
  ASSERT_NE(binding, nullptr);
  ASSERT_EQ(size, sizeof(kLocalBinding));
  EXPECT_EQ(memcmp(binding, kLocalBinding, size), 0);
  ASSERT_TRUE(store.replace(SupLanPeerStore::Source, 0,
                            kSource, sizeof(kSource)));
  ASSERT_TRUE(store.save());
  cfg.reboot();
  SupLanPeerStore restored(&cfg);
  ASSERT_TRUE(restored.load());
  EXPECT_NE(restored.get(SupLanPeerStore::Source, 0, &size), nullptr);
  EXPECT_NE(restored.get(SupLanPeerStore::Bindings, 0, &size), nullptr);
}

TEST(PeerStore, ReadErrorNeverErasesAndLaterReadCanRecover) {
  StoreConfig cfg;
  ASSERT_TRUE(cfg.setBlob(
      "sl-peers", reinterpret_cast<const char *>(kBlob), sizeof(kBlob)));
  ASSERT_TRUE(cfg.commit());
  cfg.failRead = true;
  {
    SupLanPeerStore store(&cfg);
    EXPECT_FALSE(store.load());
  }
  EXPECT_EQ(cfg.erases, 0);
  cfg.failRead = false;
  {
    SupLanPeerStore store(&cfg);
    EXPECT_TRUE(store.load());
  }
}
TEST(PeerStore, CorruptBlobErasedOnlyWhenRemovalAndCommitSucceed) {
  for (int mutation : {0, 3, static_cast<int>(sizeof(kBlob) - 1)}) {
    StoreConfig cfg;
    auto broken = std::vector<uint8_t>(kBlob, kBlob + sizeof(kBlob));
    broken[mutation] ^= 1;
    ASSERT_TRUE(cfg.setBlob(
        "sl-peers", reinterpret_cast<char *>(broken.data()), broken.size()));
    ASSERT_TRUE(cfg.commit());
    cfg.failErase = true;
    {
      SupLanPeerStore store(&cfg);
      EXPECT_FALSE(store.load());
    }
    EXPECT_GE(cfg.getBlobSize("sl-peers"), 0);
    cfg.failErase = false;
    {
      SupLanPeerStore store(&cfg);
      EXPECT_TRUE(store.load());
    }
    EXPECT_LT(cfg.getBlobSize("sl-peers"), 0);
  }
}
TEST(PeerStore, WriteAndCommitErrorsRejectTransactionAndRetryNormally) {
  StoreConfig cfg;
  for (int failure : {0, 1}) {
    SupLanPeerStore store(&cfg);
    ASSERT_TRUE(store.load(sizeof(kSource) + 4));
    ASSERT_TRUE(
        store.replace(SupLanPeerStore::Source, 0, kSource, sizeof(kSource)));
    cfg.failWrite = failure == 0;
    cfg.failCommit = failure == 1;
    EXPECT_FALSE(store.save());
    cfg.failWrite = cfg.failCommit = false;
    EXPECT_TRUE(store.save());
  }
  cfg.reboot();
  SupLanPeerStore store(&cfg);
  ASSERT_TRUE(store.load());
  size_t size;
  EXPECT_NE(store.get(SupLanPeerStore::Source, 0, &size), nullptr);
}
TEST(PeerStore, ObsoleteKeysAreDiscardedWithoutInterpretingTheirPayloads) {
  StoreConfig cfg;
  ASSERT_TRUE(cfg.setUInt32("sl-peer-254", 17));
  ASSERT_TRUE(cfg.setUInt32("slb_0", 99));
  ASSERT_TRUE(cfg.setUInt32("sl-root", 7));
  ASSERT_TRUE(cfg.setUInt32("sl-identity", 8));
  ASSERT_TRUE(cfg.commit());
  cfg.failErase = true;
  EXPECT_FALSE(SupLanPeerStore::cleanupLegacy(&cfg));
  cfg.failErase = false;
  cfg.reboot();
  const int commits = cfg.commits;
  ASSERT_TRUE(SupLanPeerStore::cleanupLegacy(&cfg));
  EXPECT_EQ(cfg.commits, commits + 1);
  uint32_t value;
  EXPECT_FALSE(cfg.getUInt32("slb_0", &value));
  EXPECT_FALSE(cfg.getUInt32("sl-peer-254", &value));
  EXPECT_TRUE(cfg.getUInt32("sl-root", &value));
  EXPECT_EQ(value, 7u);
  EXPECT_TRUE(cfg.getUInt32("sl-identity", &value));
  EXPECT_EQ(value, 8u);
}

TEST(PeerStore, FullFenceKeepsLocalBindingsAndCommitsOnce) {
  StoreConfig cfg;
  ASSERT_TRUE(cfg.setBlob(
      "sl-peers", reinterpret_cast<const char *>(kBlob), sizeof(kBlob)));
  ASSERT_TRUE(cfg.commit());
  std::array<uint8_t, Supla::Device::ServerMigration::kPrefix> fence = {};
  fence[2] = 1;
  Supla::Device::ServerMigration migration{fence.data()};
  const int commits = cfg.commits;
  {
    SupLanPeerStore store(&cfg);
    ASSERT_TRUE(store.load());
    ASSERT_TRUE(store.migrate(migration));
  }
  EXPECT_EQ(cfg.commits, commits + 1);
  cfg.reboot();
  SupLanPeerStore store(&cfg);
  ASSERT_TRUE(store.load());
  EXPECT_FALSE(store.hasServerState());
  size_t size;
  EXPECT_EQ(store.get(SupLanPeerStore::Source, 0, &size), nullptr);
  ASSERT_NE(store.get(SupLanPeerStore::Bindings, 0, &size), nullptr);
  EXPECT_EQ(size, sizeof(kLocalBinding));
}

TEST(PeerStore, OversizedBlobRejectedBeforePayloadRead) {
  StoreConfig cfg;
  std::vector<char> huge(SupLanPeerStore::kMaximum + 1);
  ASSERT_TRUE(cfg.setBlob("sl-peers", huge.data(), huge.size()));
  ASSERT_TRUE(cfg.commit());
  const int reads = cfg.reads;
  SupLanPeerStore store(&cfg);
  ASSERT_TRUE(store.load());
  EXPECT_EQ(cfg.reads, reads);
  EXPECT_LT(cfg.getBlobSize("sl-peers"), 0);
}

TEST(PeerStore, InterruptedLegacyCommitRetriesWithoutTouchingIdentityKeys) {
  StoreConfig cfg;
  ASSERT_TRUE(cfg.setUInt32("slb_1", 1));
  ASSERT_TRUE(cfg.setUInt32("sl-service", 99));
  ASSERT_TRUE(cfg.commit());
  bool pending = false;
  cfg.failCommit = true;
  EXPECT_FALSE(SupLanPeerStore::cleanupLegacy(&cfg, &pending));
  EXPECT_TRUE(pending);
  cfg.failCommit = false;
  EXPECT_TRUE(SupLanPeerStore::cleanupLegacy(&cfg, &pending));
  EXPECT_FALSE(pending);
  cfg.reboot();
  uint32_t value;
  EXPECT_FALSE(cfg.getUInt32("slb_1", &value));
  EXPECT_TRUE(cfg.getUInt32("sl-service", &value));
  EXPECT_EQ(value, 99u);
}

TEST(PeerStore, AllIndependentProfilePoolsFitAndOneMoreBindingIsRejected) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  StoreConfig cfg;
  SupLanPeerStore store(&cfg);
  ASSERT_TRUE(store.load(SupLanPeerStore::kMaximum));
  unsigned remainingAcl = SUPLAN_MAX_TOTAL_ACL_ENTRIES;
  unsigned remainingExpected = SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES;
  const unsigned sourceSlots = (remainingAcl + 88) / 89;
  ASSERT_LT(sourceSlots, SUPLAN_MAX_PERSISTENT_PEERS);
  for (unsigned slot = 0; slot < SUPLAN_MAX_PERSISTENT_PEERS; ++slot) {
    const bool source = slot < sourceSlots;
    auto &remaining = source ? remainingAcl : remainingExpected;
    const unsigned peersLeft =
        source ? sourceSlots - slot : SUPLAN_MAX_PERSISTENT_PEERS - slot;
    const unsigned count = std::min(89U, remaining - peersLeft + 1);
    remaining -= count;
    const unsigned prefix = source ? 34 : 66;
    std::vector<uint8_t> record(prefix + 6 * count + 2);
    memcpy(record.data(), source ? kSource : kDestination, prefix);
    putUint32(record.data() + 17, 1000 + slot);
    record[33] = count;
    for (unsigned i = 0; i < count; ++i) {
      record[prefix + 6 * i] = 1;
      putUint32(record.data() + prefix + 6 * i + 1, 10000 + i);
      record[prefix + 6 * i + 5] = 1;
    }
    PeerStoreTest::half(record.data() + record.size() - 2,
                        calculateCrc16(record.data(), record.size() - 2));
    ASSERT_TRUE(store.replace(
        source ? SupLanPeerStore::Source : SupLanPeerStore::Destination,
        slot,
        record.data(),
        record.size()));
  }
  EXPECT_EQ(remainingAcl, 0u);
  EXPECT_EQ(remainingExpected, 0u);
  unsigned remaining = SUPLAN_MAX_RESOURCE_BINDINGS;
  const unsigned sets = std::min<unsigned>(SUPLAN_MAX_PERSISTENT_PEERS,
                                           SUPLAN_MAX_RESOURCE_BINDINGS);
  std::vector<uint8_t> last;
  for (unsigned slot = 0; slot < sets; ++slot) {
    const unsigned count = remaining / (sets - slot);
    remaining -= count;
    ASSERT_LE(count, SUPLAN_MAX_BINDINGS_PER_PEER);
    std::vector<uint8_t> record(29 + 12 * count);
    memcpy(record.data(), kLocalBinding, 29);
    putUint32(record.data() + 11, slot + 1);
    putUint32(record.data() + 16, slot + 2);
    record[28] = count;
    for (unsigned i = 0; i < count; ++i) {
      memcpy(record.data() + 29 + 12 * i, kLocalBinding + 29, 12);
      putUint32(record.data() + 31 + 12 * i, 10000 + i);
    }
    ASSERT_TRUE(store.replace(
        SupLanPeerStore::Bindings, slot, record.data(), record.size()));
    last = record;
  }
  ASSERT_TRUE(store.save());
  const int size = cfg.getBlobSize("sl-peers");
  EXPECT_LE(size, SupLanPeerStore::kMaximum);
  cfg.reboot();
  {
    SupLanPeerStore restored(&cfg);
    ASSERT_TRUE(restored.load());
  }
  last.insert(last.end(), kLocalBinding + 29, kLocalBinding + 41);
  last[28]++;
  putUint32(last.data() + last.size() - 10, 99999);
  ASSERT_TRUE(store.replace(
      SupLanPeerStore::Bindings, sets - 1, last.data(), last.size()));
  EXPECT_FALSE(store.save());
  EXPECT_EQ(cfg.getBlobSize("sl-peers"), size);
  const auto selector = std::string("slb_") + std::to_string(sets - 1);
  ASSERT_TRUE(PeerStoreTest::write(cfg,
                                   selector.c_str(),
                                   reinterpret_cast<char *>(last.data()),
                                   last.size()));
  ASSERT_TRUE(cfg.commit());
  SupLanPeerStore rejected(&cfg);
  ASSERT_TRUE(rejected.load());
  EXPECT_LT(cfg.getBlobSize("sl-peers"), 0);
}

TEST(PeerStore, DuplicateRecordWithValidCrcDeletesWholeBlob) {
  StoreConfig cfg;
  auto duplicate = std::vector<uint8_t>(kBlob, kBlob + sizeof(kBlob) - 2);
  const size_t recordSize = 3 + PeerStoreTest::half(kBlob + 8);
  duplicate.insert(duplicate.end(), kBlob + 7, kBlob + 7 + recordSize);
  duplicate.resize(duplicate.size() + 2);
  PeerStoreTest::half(duplicate.data() + 1, 4);
  PeerStoreTest::half(duplicate.data() + 3, duplicate.size());
  PeerStoreTest::half(duplicate.data() + duplicate.size() - 2,
                      calculateCrc16(duplicate.data(), duplicate.size() - 2));
  ASSERT_TRUE(cfg.setBlob("sl-peers",
                          reinterpret_cast<char *>(duplicate.data()),
                          duplicate.size()));
  ASSERT_TRUE(cfg.commit());
  SupLanPeerStore store(&cfg);
  ASSERT_TRUE(store.load());
  EXPECT_FALSE(store.hasBindings());
  EXPECT_FALSE(store.hasServerState());
  EXPECT_LT(cfg.getBlobSize("sl-peers"), 0);
}
}  // namespace

TEST(PeerStore, ReadOnlyInspectionLeavesCorruptBlobForNormalLoader) {
  for (bool oversized : {false, true}) {
    StoreConfig cfg;
    std::vector<uint8_t> broken(kBlob, kBlob + sizeof(kBlob));
    if (oversized) broken.resize(SupLanPeerStore::kMaximum + 1);
    else broken.back() ^= 1;
    ASSERT_TRUE(cfg.setBlob("sl-peers",
        reinterpret_cast<const char *>(broken.data()), broken.size()));
    ASSERT_TRUE(cfg.commit());
    const int commits = cfg.commits;
    {
      SupLanPeerStore inspect(&cfg);
      EXPECT_FALSE(inspect.load(0, false));
    }
    EXPECT_EQ(cfg.erases, 0);
    EXPECT_EQ(cfg.commits, commits);
    EXPECT_EQ(cfg.getBlobSize("sl-peers"), broken.size());
    SupLanPeerStore loader(&cfg);
    EXPECT_TRUE(loader.load());
    EXPECT_EQ(cfg.erases, 1);
    EXPECT_EQ(cfg.commits, commits + 1);
    EXPECT_LT(cfg.getBlobSize("sl-peers"), 0);
  }
}
