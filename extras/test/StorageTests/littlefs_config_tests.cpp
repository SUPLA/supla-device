// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <config_key_collector.h>
#include <LittleFS.h>
#include <supla/storage/littlefs_config.h>
#include <supla/suplan/suplan_server_migration.h>

#include <array>
#include <cstring>

namespace {

constexpr char BlobKey[] = "blob";

void writeFile(const char *path, const uint8_t *data, size_t size) {
  File file = LittleFS.open(path, "w");
  ASSERT_TRUE(static_cast<bool>(file));
  ASSERT_EQ(file.write(data, size), size);
  file.close();
}

class LittleFsConfigTests : public ::testing::Test {
 protected:
  void SetUp() override { LittleFS.reset(); }
  void TearDown() override { LittleFS.reset(); }
};

}  // namespace

TEST_F(LittleFsConfigTests, GetBlobReadsKeyValueBlobIntoLargerBuffer) {
  Supla::LittleFsConfig config;
  const std::array<char, 10> stored = {'0', '1', '2', '3', '4',
                                       '5', '6', '7', '8', '9'};
  std::array<char, 60> loaded;
  loaded.fill('x');

  ASSERT_TRUE(config.setBlob(BlobKey, stored.data(), stored.size()));
  ASSERT_TRUE(config.getBlob(BlobKey, loaded.data(), loaded.size()));

  EXPECT_EQ(std::memcmp(loaded.data(), stored.data(), stored.size()), 0);
  for (size_t i = stored.size(); i < loaded.size(); i++) {
    EXPECT_EQ(loaded[i], 'x');
  }
}

TEST_F(LittleFsConfigTests, GetBlobReadsFileBlobIntoLargerBuffer) {
  Supla::LittleFsConfig config;
  const std::array<char, 60> stored = {};
  std::array<char, 80> loaded;
  loaded.fill('x');

  ASSERT_TRUE(config.setBlob(BlobKey, stored.data(), stored.size()));
  ASSERT_TRUE(LittleFS.exists("/supla/blob"));
  ASSERT_TRUE(config.getBlob(BlobKey, loaded.data(), loaded.size()));

  EXPECT_EQ(std::memcmp(loaded.data(), stored.data(), stored.size()), 0);
  for (size_t i = stored.size(); i < loaded.size(); i++) {
    EXPECT_EQ(loaded[i], 'x');
  }
}

TEST_F(LittleFsConfigTests, GetBlobRejectsFileBlobLargerThanBuffer) {
  Supla::LittleFsConfig config;
  const std::array<char, 60> stored = {};
  std::array<char, 10> loaded;
  loaded.fill('x');

  ASSERT_TRUE(config.setBlob(BlobKey, stored.data(), stored.size()));
  EXPECT_FALSE(config.getBlob(BlobKey, loaded.data(), loaded.size()));
  for (const auto value : loaded) {
    EXPECT_EQ(value, 'x');
  }
}

TEST_F(LittleFsConfigTests, SetBlobSwitchesBetweenKeyValueAndFileStorage) {
  Supla::LittleFsConfig config;
  const std::array<char, 10> smallBlob = {};
  const std::array<char, 60> largeBlob = {};
  std::array<char, 60> loaded;

  ASSERT_TRUE(config.setBlob(BlobKey, smallBlob.data(), smallBlob.size()));
  ASSERT_TRUE(config.setBlob(BlobKey, largeBlob.data(), largeBlob.size()));
  ASSERT_TRUE(LittleFS.exists("/supla/blob"));

  loaded.fill('x');
  ASSERT_TRUE(config.getBlob(BlobKey, loaded.data(), loaded.size()));
  EXPECT_EQ(std::memcmp(loaded.data(), largeBlob.data(), largeBlob.size()), 0);

  ASSERT_TRUE(config.setBlob(BlobKey, smallBlob.data(), smallBlob.size()));
  EXPECT_FALSE(LittleFS.exists("/supla/blob"));
  loaded.fill('x');
  ASSERT_TRUE(config.getBlob(BlobKey, loaded.data(), loaded.size()));
  EXPECT_EQ(std::memcmp(loaded.data(), smallBlob.data(), smallBlob.size()), 0);
  for (size_t i = smallBlob.size(); i < loaded.size(); i++) {
    EXPECT_EQ(loaded[i], 'x');
  }
}

TEST_F(LittleFsConfigTests, RemoveAllDeletesEveryFileBlob) {
  Supla::LittleFsConfig config;
  const std::array<char, 60> stored = {};

  ASSERT_TRUE(config.setBlob("blob_1", stored.data(), stored.size()));
  ASSERT_TRUE(config.setBlob("blob_2", stored.data(), stored.size()));
  ASSERT_TRUE(LittleFS.exists("/supla/blob_1"));
  ASSERT_TRUE(LittleFS.exists("/supla/blob_2"));

  ASSERT_TRUE(config.removeAllChecked());

  EXPECT_FALSE(LittleFS.exists("/supla/blob_1"));
  EXPECT_FALSE(LittleFS.exists("/supla/blob_2"));
}

TEST_F(LittleFsConfigTests, FailedResetEraseIsObservable) {
  Supla::LittleFsConfig config;
  const char stored[80] = {42};
  ASSERT_TRUE(config.setBlob("sl-peer-0", stored, sizeof(stored)));
  ASSERT_TRUE(LittleFS.exists("/supla/sl-peer-0"));
  LittleFS.failRemovePath = "/supla/sl-peer-0";
  EXPECT_FALSE(config.removeAllChecked());
  EXPECT_TRUE(LittleFS.exists("/supla/sl-peer-0"));
  LittleFS.failRemovePath.clear();
  EXPECT_TRUE(config.removeAllChecked());
  EXPECT_FALSE(LittleFS.exists("/supla/sl-peer-0"));
}

TEST_F(LittleFsConfigTests, FallsBackToBackupAfterPartialPrimaryParse) {
  constexpr size_t firstRecordSize = SUPLA_STORAGE_KEY_SIZE + 1 + 2 + 1;
  constexpr size_t secondRecordHeaderAndOneByte =
      SUPLA_STORAGE_KEY_SIZE + 1 + 2 + 1;

  Supla::LittleFsConfig primarySource;
  ASSERT_TRUE(primarySource.setUInt8("primary_only", 1));
  ASSERT_TRUE(primarySource.setUInt32("shared", 111));
  std::array<uint8_t, SUPLA_LITTLEFS_CONFIG_BUF_SIZE> primaryBuffer = {};
  const size_t primarySize = primarySource.serializeToMemory(
      primaryBuffer.data(), primaryBuffer.size());
  const size_t corruptPrimarySize =
      firstRecordSize + secondRecordHeaderAndOneByte;
  ASSERT_GT(primarySize, corruptPrimarySize);

  Supla::LittleFsConfig backupSource;
  ASSERT_TRUE(backupSource.setUInt32("shared", 222));
  ASSERT_TRUE(backupSource.setUInt8("backup_only", 2));
  std::array<uint8_t, SUPLA_LITTLEFS_CONFIG_BUF_SIZE> backupBuffer = {};
  const size_t backupSize =
      backupSource.serializeToMemory(backupBuffer.data(), backupBuffer.size());

  writeFile("/supla-dev.cfg", primaryBuffer.data(), corruptPrimarySize);
  writeFile("/supla-dev.cfg.bak", backupBuffer.data(), backupSize);

  Supla::LittleFsConfig config;
  ASSERT_TRUE(config.init());

  uint32_t shared = 0;
  EXPECT_TRUE(config.getUInt32("shared", &shared));
  EXPECT_EQ(shared, 222);

  uint8_t backupOnly = 0;
  EXPECT_TRUE(config.getUInt8("backup_only", &backupOnly));
  EXPECT_EQ(backupOnly, 2);

  uint8_t primaryOnly = 0;
  EXPECT_FALSE(config.getUInt8("primary_only", &primaryOnly));
}

TEST_F(LittleFsConfigTests, ValidPrimaryConfigurationStillWins) {
  Supla::LittleFsConfig primarySource;
  ASSERT_TRUE(primarySource.setUInt32("shared", 111));
  std::array<uint8_t, SUPLA_LITTLEFS_CONFIG_BUF_SIZE> primaryBuffer = {};
  const size_t primarySize = primarySource.serializeToMemory(
      primaryBuffer.data(), primaryBuffer.size());

  Supla::LittleFsConfig backupSource;
  ASSERT_TRUE(backupSource.setUInt32("shared", 222));
  std::array<uint8_t, SUPLA_LITTLEFS_CONFIG_BUF_SIZE> backupBuffer = {};
  const size_t backupSize =
      backupSource.serializeToMemory(backupBuffer.data(), backupBuffer.size());

  writeFile("/supla-dev.cfg", primaryBuffer.data(), primarySize);
  writeFile("/supla-dev.cfg.bak", backupBuffer.data(), backupSize);

  Supla::LittleFsConfig config;
  ASSERT_TRUE(config.init());

  uint32_t shared = 0;
  EXPECT_TRUE(config.getUInt32("shared", &shared));
  EXPECT_EQ(shared, 111);
}

TEST_F(LittleFsConfigTests, CommitReportsMountOpenShortWriteAndBufferFailures) {
  Supla::LittleFsConfig config(64);
  ASSERT_TRUE(config.setUInt32("test", 42));
  LittleFS.beginResult = false;
  EXPECT_FALSE(config.commit());
  LittleFS.beginResult = true;
  LittleFS.failOpenPath = "/supla-dev.cfg";
  EXPECT_FALSE(config.commit());
  LittleFS.failOpenPath.clear();
  LittleFS.maxWrite = 1;
  EXPECT_FALSE(config.commit());
  LittleFS.maxWrite = SIZE_MAX;
  EXPECT_TRUE(config.commit());
  Supla::LittleFsConfig tooSmall(8);
  ASSERT_TRUE(tooSmall.setUInt32("test", 42));
  EXPECT_FALSE(tooSmall.commit());
}

TEST_F(LittleFsConfigTests, CommitRejectsLostCloseAndSyncOnBothFiles) {
  Supla::LittleFsConfig config(64);
  ASSERT_TRUE(config.setUInt32("test", 42));
  for (const char *path : {"/supla-dev.cfg", "/supla-dev.cfg.bak"}) {
    for (bool syncFailure : {false, true}) {
      LittleFS.reset();
      if (syncFailure) {
        LittleFS.failSyncPath = path;
      } else {
        LittleFS.failClosePath = path;
      }
      EXPECT_FALSE(config.commit()) << path << ": sync=" << syncFailure;
      LittleFS.failSyncPath.clear();
      LittleFS.failClosePath.clear();
      EXPECT_TRUE(config.commit());
    }
  }
}

TEST_F(LittleFsConfigTests, BlobRejectsLostFinalizationWithSameSizeOldFile) {
  Supla::LittleFsConfig config;
  std::array<char, 39> old = {};
  std::array<char, 39> changed;
  changed.fill('x');
  for (bool syncFailure : {false, true}) {
    ASSERT_TRUE(config.setBlob(BlobKey, old.data(), old.size()));
    if (syncFailure) {
      LittleFS.failSyncPath = "/supla/blob";
    } else {
      LittleFS.failClosePath = "/supla/blob";
    }
    EXPECT_FALSE(config.setBlob(BlobKey, changed.data(), changed.size()));
    std::array<char, 39> loaded;
    ASSERT_TRUE(config.getBlob(BlobKey, loaded.data(), loaded.size()));
    EXPECT_EQ(loaded, old);
    LittleFS.failSyncPath.clear();
    LittleFS.failClosePath.clear();
    ASSERT_TRUE(config.setBlob(BlobKey, changed.data(), changed.size()));
    ASSERT_TRUE(config.getBlob(BlobKey, loaded.data(), loaded.size()));
    EXPECT_EQ(loaded, changed);
  }
}

TEST_F(LittleFsConfigTests, VerificationRejectsMountReadAndContentFailures) {
  Supla::LittleFsConfig config;
  std::array<char, 100> data;
  data.fill('x');
  for (int failure = 0; failure < 4; ++failure) {
    LittleFS.reset();
    if (failure == 0) {
      // Verification mount, after successful write.
      LittleFS.failBeginCall = 2;
    } else if (failure == 1) {
      LittleFS.failReadPath = "/supla/blob";
    } else if (failure == 2) {
      LittleFS.maxRead = 1;
    } else {
      LittleFS.corruptClosePath = "/supla/blob";
    }
    EXPECT_FALSE(config.setBlob(BlobKey, data.data(), data.size())) << failure;
    // No format on verification failure.
    EXPECT_TRUE(LittleFS.exists("/supla/blob"));
  }
}

TEST_F(LittleFsConfigTests, CommitRejectsSameSizeCorruptionAfterClose) {
  Supla::LittleFsConfig config(64);
  ASSERT_TRUE(config.setUInt32("test", 42));
  for (const char *path : {"/supla-dev.cfg", "/supla-dev.cfg.bak"}) {
    LittleFS.reset();
    LittleFS.corruptClosePath = path;
    EXPECT_FALSE(config.commit()) << path;
  }
}

TEST_F(LittleFsConfigTests, EraseKeyConfirmsFileDeletionAcrossRemount) {
  Supla::LittleFsConfig config;
  std::array<char, 60> stored = {};
  ASSERT_TRUE(config.setBlob(BlobKey, stored.data(), stored.size()));
  LittleFS.failRemovePath = "/supla/blob";
  EXPECT_FALSE(config.eraseKey(BlobKey));
  EXPECT_TRUE(LittleFS.exists("/supla/blob"));
  LittleFS.failRemovePath.clear();
  EXPECT_TRUE(config.eraseKey(BlobKey));
  LittleFS.end();
  ASSERT_TRUE(LittleFS.begin());
  EXPECT_FALSE(LittleFS.exists("/supla/blob"));
}
TEST_F(LittleFsConfigTests, EraseKeyVerificationFailureDoesNotAutoFormat) {
  Supla::LittleFsConfig config;
  std::array<char, 60> stored = {};
  ASSERT_TRUE(config.setBlob(BlobKey, stored.data(), stored.size()));
  ASSERT_TRUE(config.setBlob("other", stored.data(), stored.size()));
  LittleFS.failBeginCall = LittleFS.beginCalls + 2;
  EXPECT_FALSE(config.eraseKey(BlobKey));
  EXPECT_TRUE(LittleFS.exists("/supla/other"));
}

TEST_F(LittleFsConfigTests, IteratesMemoryAndFilesOnceWithSubstringFilter) {
  Supla::LittleFsConfig config;
  ASSERT_TRUE(config.setUInt32("other", 42));
  ASSERT_TRUE(config.setBlob("1_hvac_cfg2", "abc", 3));
  const std::array<char, 60> blob = {};
  ASSERT_TRUE(config.setBlob("254_hvac_cfg2", blob.data(), blob.size()));
  ASSERT_TRUE(config.setBlob("sl-peer-254", blob.data(), blob.size()));
  // Simulate a stale file shadowed by a current in-memory value.
  const uint8_t data[] = {1, 2, 3};
  writeFile("/supla/1_hvac_cfg2", data, sizeof(data));
  const int mounts = LittleFS.beginCalls;
  ConfigKeyCollector found;
  EXPECT_EQ(config.forEachKey("hvac_", found.collect, &found),
            Supla::ConfigIterationResult::COMPLETED);
  EXPECT_EQ(LittleFS.beginCalls, mounts + 1);
  EXPECT_EQ(found.visits, 2);
  EXPECT_EQ(found.keys.size(), 2u);
  EXPECT_EQ(found.keys.at("254_hvac_cfg2"), Supla::ConfigValueType::BLOB);
  ConfigKeyCollector all;
  EXPECT_EQ(config.forEachKey("", all.collect, &all),
            Supla::ConfigIterationResult::COMPLETED);
  EXPECT_EQ(all.visits, 4);
}

TEST_F(LittleFsConfigTests, StopFromMemoryDoesNotMountAndStopFromFileWorks) {
  Supla::LittleFsConfig config;
  ASSERT_TRUE(config.setUInt8("memory", 1));
  const std::array<char, 60> blob = {};
  ASSERT_TRUE(config.setBlob("file", blob.data(), blob.size()));
  ConfigKeyCollector found;
  found.stopAfter = 1;
  int mounts = LittleFS.beginCalls;
  EXPECT_EQ(config.forEachKey("memory", found.collect, &found),
            Supla::ConfigIterationResult::STOPPED);
  EXPECT_EQ(LittleFS.beginCalls, mounts);
  found = {};
  found.stopAfter = 1;
  EXPECT_EQ(config.forEachKey("file", found.collect, &found),
            Supla::ConfigIterationResult::STOPPED);
  EXPECT_EQ(found.visits, 1);
  // Iterator cleanup must leave regular Config access usable.
  std::array<char, 60> loaded = {};
  EXPECT_TRUE(config.getBlob("file", loaded.data(), loaded.size()));
}

TEST_F(LittleFsConfigTests, EmptyFailureAndInvalidArgumentsAreDistinct) {
  Supla::LittleFsConfig config;
  ConfigKeyCollector found;
  using Result = Supla::ConfigIterationResult;
  EXPECT_EQ(config.forEachKey("", found.collect, &found), Result::COMPLETED);
  EXPECT_EQ(found.visits, 0);
  EXPECT_EQ(config.forEachKey(nullptr, found.collect, &found), Result::ERROR);
  EXPECT_EQ(config.forEachKey("", nullptr, &found), Result::ERROR);
  const uint8_t data[] = {42};
  writeFile("/supla/preserved", data, sizeof(data));
  LittleFS.beginResult = false;
  EXPECT_EQ(config.forEachKey("", found.collect, &found), Result::ERROR);
  EXPECT_TRUE(LittleFS.exists("/supla/preserved"));  // no implicit format
  LittleFS.beginResult = true;
  LittleFS.failOpenPath = "/supla";
  EXPECT_EQ(config.forEachKey("", found.collect, &found), Result::ERROR);
}

TEST_F(LittleFsConfigTests,
       MigrationDetectionMountsOnceInsteadOfProbing1275Keys) {
  Supla::LittleFsConfig config;
  int mounts = LittleFS.beginCalls;
  EXPECT_FALSE(Supla::Device::ServerMigration::hasServerState(&config));
  EXPECT_EQ(LittleFS.beginCalls, mounts + 1);
  const std::array<char, 60> peer = {};
  ASSERT_TRUE(config.setBlob("sl-peer-254", peer.data(), peer.size()));
  mounts = LittleFS.beginCalls;
  EXPECT_TRUE(Supla::Device::ServerMigration::hasServerState(&config));
  EXPECT_EQ(LittleFS.beginCalls, mounts + 1);
}

TEST_F(LittleFsConfigTests, UnreadableUnrelatedFileDoesNotHideServerPeer) {
  Supla::LittleFsConfig config;
  const std::array<char, 60> blob = {};
  ASSERT_TRUE(config.setBlob("aaa", blob.data(), blob.size()));
  ASSERT_TRUE(config.setBlob("sl-peer-254", blob.data(), blob.size()));
  LittleFS.failOpenPath = "/supla/aaa";
  ConfigKeyCollector found;
  EXPECT_EQ(config.forEachKey("", found.collect, &found),
            Supla::ConfigIterationResult::COMPLETED);
  EXPECT_EQ(found.visits, 2);
  EXPECT_EQ(found.keys.at("sl-peer-254"), Supla::ConfigValueType::BLOB);
  EXPECT_TRUE(Supla::Device::ServerMigration::hasServerState(&config));
  EXPECT_EQ(LittleFS.openDirectories, 0);
  std::array<char, 60> readable = {};
  EXPECT_TRUE(config.getBlob("sl-peer-254", readable.data(), readable.size()));
}

TEST_F(LittleFsConfigTests, DirectoryReadAndCloseErrorsCannotReportCompletion) {
  Supla::LittleFsConfig config;
  const std::array<char, 60> blob = {};
  ASSERT_TRUE(config.setBlob("aaa", blob.data(), blob.size()));
  ASSERT_TRUE(config.setBlob("sl-peer-254", blob.data(), blob.size()));
  // Before/after entries and at expected EOF.
  for (int failingRead : {1, 2, 3}) {
    LittleFS.directoryReadCalls = 0;
    LittleFS.failDirectoryReadCall = failingRead;
    ConfigKeyCollector found;
    EXPECT_EQ(config.forEachKey("", found.collect, &found),
              Supla::ConfigIterationResult::ERROR);
    EXPECT_EQ(LittleFS.openDirectories, 0);
    LittleFS.directoryReadCalls = 0;
    EXPECT_TRUE(Supla::Device::ServerMigration::hasServerState(&config));
  }
  LittleFS.failDirectoryReadCall = -1;
  LittleFS.failDirectoryClose = true;
  ConfigKeyCollector found;
  EXPECT_EQ(config.forEachKey("", found.collect, &found),
            Supla::ConfigIterationResult::ERROR);
  EXPECT_EQ(LittleFS.openDirectories, 0);
  EXPECT_TRUE(Supla::Device::ServerMigration::hasServerState(&config));
}

TEST_F(LittleFsConfigTests,
       MissingDirectoryIsEmptyAndStopClosesNativeIterator) {
  Supla::LittleFsConfig config;
  ConfigKeyCollector found;
  LittleFS.directoryMissing = true;
  EXPECT_EQ(config.forEachKey("", found.collect, &found),
            Supla::ConfigIterationResult::COMPLETED);
  EXPECT_EQ(found.visits, 0);
  LittleFS.directoryMissing = false;
  const std::array<char, 60> blob = {};
  ASSERT_TRUE(config.setBlob("sl-peer-254", blob.data(), blob.size()));
  found.stopAfter = 1;
  EXPECT_EQ(config.forEachKey("", found.collect, &found),
            Supla::ConfigIterationResult::STOPPED);
  EXPECT_EQ(found.visits, 1);
  EXPECT_EQ(LittleFS.openDirectories, 0);
}
