// SPDX-FileCopyrightText: AC SOFTWARE SP. Z. O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <linux_file_state_logger.h>
#include <linux_file_storage.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <string>
#include <vector>

namespace {

class UmaskGuard {
 public:
  explicit UmaskGuard(mode_t mask) : originalMask(::umask(mask)) {
  }

  ~UmaskGuard() {
    ::umask(originalMask);
  }

 private:
  mode_t originalMask;
};

class TestLinuxFileStorage : public Supla::LinuxFileStorage {
 public:
  explicit TestLinuxFileStorage(const std::string& path)
      : Supla::LinuxFileStorage(path) {
  }

  using Supla::LinuxFileStorage::writeStorage;
};

class Sd4linuxFilePermissionTests : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto directoryTemplate =
        std::filesystem::temp_directory_path() /
        ("supla_file_permissions_tests_" + std::to_string(getpid()) +
         "_XXXXXX");
    std::string writableDirectoryTemplate = directoryTemplate.string();
    std::vector<char> writableDirectory(writableDirectoryTemplate.begin(),
                                        writableDirectoryTemplate.end());
    writableDirectory.push_back('\0');

    ASSERT_NE(mkdtemp(writableDirectory.data()), nullptr);
    tempDirectory = writableDirectory.data();
  }

  void TearDown() override {
    if (tempDirectory.empty()) {
      return;
    }

    std::error_code error;
    std::filesystem::remove_all(tempDirectory, error);
    EXPECT_FALSE(error) << error.message();
  }

  std::filesystem::path tempDirectory;
};

}  // namespace

TEST_F(Sd4linuxFilePermissionTests,
       CreatesLastStateAndStorageFilesWithOwnerOnlyPermissions) {
  UmaskGuard umaskGuard(0022);
  const auto stateDirectory = tempDirectory / "state";
  const auto lastStatePath = stateDirectory / "last_state.txt";
  const auto storagePath = stateDirectory / "state.bin";

  Supla::Device::FileStateLogger logger(stateDirectory.string());

  struct stat lastStateStat = {};
  ASSERT_EQ(::stat(lastStatePath.c_str(), &lastStateStat), 0);
  EXPECT_EQ(lastStateStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), 0600);

  TestLinuxFileStorage storage(stateDirectory.string());
  ASSERT_TRUE(storage.init());
  unsigned char value = 1;
  ASSERT_EQ(storage.writeStorage(0, &value, 1), 1);
  storage.commit();

  struct stat storageStat = {};
  ASSERT_EQ(::stat(storagePath.c_str(), &storageStat), 0);
  EXPECT_EQ(storageStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), 0600);
}

TEST_F(Sd4linuxFilePermissionTests,
       RewritesExistingLastStateAndStorageFilesWithOwnerOnlyPermissions) {
  const auto stateDirectory = tempDirectory / "state";
  const auto lastStatePath = stateDirectory / "last_state.txt";
  const auto storagePath = stateDirectory / "state.bin";
  ASSERT_TRUE(std::filesystem::create_directories(stateDirectory));

  {
    std::ofstream lastState(lastStatePath);
    ASSERT_TRUE(lastState.is_open());
    lastState << "old state";
  }
  {
    std::ofstream storageFile(storagePath, std::ios::binary);
    ASSERT_TRUE(storageFile.is_open());
    storageFile << "old storage";
  }
  ASSERT_EQ(::chmod(lastStatePath.c_str(), 0644), 0);
  ASSERT_EQ(::chmod(storagePath.c_str(), 0644), 0);

  Supla::Device::FileStateLogger logger(stateDirectory.string());
  struct stat lastStateStat = {};
  ASSERT_EQ(::stat(lastStatePath.c_str(), &lastStateStat), 0);
  EXPECT_EQ(lastStateStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), 0600);

  TestLinuxFileStorage storage(stateDirectory.string());
  ASSERT_TRUE(storage.init());
  unsigned char value = 2;
  ASSERT_EQ(storage.writeStorage(0, &value, 1), 1);
  storage.commit();

  struct stat storageStat = {};
  ASSERT_EQ(::stat(storagePath.c_str(), &storageStat), 0);
  EXPECT_EQ(storageStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), 0600);
}
