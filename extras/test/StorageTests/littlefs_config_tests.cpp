// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <LittleFS.h>
#include <supla/storage/littlefs_config.h>

#include <array>
#include <cstring>

namespace {

constexpr char BlobKey[] = "blob";

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

  config.removeAll();

  EXPECT_FALSE(LittleFS.exists("/supla/blob_1"));
  EXPECT_FALSE(LittleFS.exists("/supla/blob_2"));
}
