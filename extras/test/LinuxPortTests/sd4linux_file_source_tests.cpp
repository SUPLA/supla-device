// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/source/file.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <string>
#include <vector>

extern "C" const char *supla_test_get_last_log();
extern "C" void supla_test_clear_last_log();

namespace {

constexpr size_t maxContentSize = 10 * 1024 * 1024;

class InspectableFile : public Supla::Source::File {
 public:
  using File::File;
  const std::string &previous() const { return prevResult; }
  int failures() const { return readFailures; }
};

class FileSourceTests : public testing::Test {
 protected:
  void SetUp() override {
    const auto pattern = (std::filesystem::temp_directory_path() /
                          "sd4linux-file-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    const int fd = mkstemp(name.data());
    ASSERT_GE(fd, 0);
    close(fd);
    path = name.data();
  }

  void TearDown() override {
    if (!path.empty()) {
      std::filesystem::remove(path);
    }
  }

  void write(const std::string &content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(content.data(), content.size());
    ASSERT_TRUE(out.good());
  }

  void writeRepeated(size_t count, char value) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    std::array<char, 4096> block;
    block.fill(value);
    while (count > 0) {
      const auto size = std::min(count, block.size());
      out.write(block.data(), size);
      ASSERT_TRUE(out.good());
      count -= size;
    }
  }

  void appendNewline() {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    out.put('\n');
    ASSERT_TRUE(out.good());
  }

  std::string path;
};

}  // namespace

TEST_F(FileSourceTests, EmptyFileRetainsExistingFailurePolicy) {
  InspectableFile source(path.c_str(), 0);
  for (int i = 1; i <= 4; ++i) {
    EXPECT_TRUE(source.getContent().empty());
    EXPECT_EQ(source.failures(), i % 4);
  }
  EXPECT_TRUE(source.previous().empty());
  EXPECT_TRUE(source.isConnected());
}

TEST_F(FileSourceTests, PreservesLinesEmptyLinesCrLfAndFinalNewline) {
  InspectableFile source(path.c_str(), 0);
  const std::vector<std::string> cases = {
      "one", "one\n", "one\n\nthree", "\n\none\n\n",
      "one\r\n\r\nthree\r", "\n", std::string("a\0b\n", 4)};
  for (const auto &content : cases) {
    write(content);
    const auto expected = content.back() == '\n' ? content : content + "\n";
    EXPECT_EQ(source.getContent(), expected);
    EXPECT_EQ(source.failures(), 0);
  }
}

TEST_F(FileSourceTests, AcceptsExactlyTenMiBIncludingSynthesizedNewline) {
  InspectableFile source(path.c_str(), 0);
  writeRepeated(maxContentSize - 1, 'x');
  for (bool terminated : {false, true}) {
    if (terminated) {
      appendNewline();
    }
    const auto result = source.getContent();
    ASSERT_EQ(result.size(), maxContentSize);
    EXPECT_EQ(result.back(), '\n');
    EXPECT_TRUE(std::all_of(result.begin(), result.end() - 1,
                            [](char c) { return c == 'x'; }));
    EXPECT_EQ(source.failures(), 0);
  }
}

TEST_F(FileSourceTests, RejectsOneByteOverLimitWithoutPartialContent) {
  InspectableFile source(path.c_str(), 0);
  writeRepeated(maxContentSize, 'x');
  appendNewline();
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_TRUE(source.previous().empty());
  EXPECT_EQ(source.failures(), 1);
}

TEST_F(FileSourceTests, RejectsTenMiBWhenFinalNewlineWouldExceedLimit) {
  InspectableFile source(path.c_str(), 0);
  writeRepeated(maxContentSize, 'x');
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_TRUE(source.previous().empty());
  EXPECT_EQ(source.failures(), 1);
}

TEST_F(FileSourceTests, RejectsTwentyMiBLineWrittenInSmallBlocks) {
  InspectableFile source(path.c_str(), 0);
  writeRepeated(2 * maxContentSize, 'x');
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_TRUE(source.previous().empty());
  EXPECT_EQ(source.failures(), 1);
}

TEST_F(FileSourceTests, RejectsTooManyShortLines) {
  InspectableFile source(path.c_str(), 0);
  writeRepeated(maxContentSize + 1, '\n');
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_TRUE(source.previous().empty());
  EXPECT_EQ(source.failures(), 1);
}

TEST_F(FileSourceTests, LimitFailurePreservesCacheAndRecoveryResetsFailures) {
  InspectableFile source(path.c_str(), 0);
  write("previous");
  ASSERT_EQ(source.getContent(), "previous\n");
  writeRepeated(2 * maxContentSize, 'x');
  for (int i = 1; i <= 3; ++i) {
    EXPECT_EQ(source.getContent(), "previous\n");
    EXPECT_EQ(source.failures(), i);
    EXPECT_EQ(source.previous(), "previous\n");
  }
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_EQ(source.failures(), 3);
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_EQ(source.previous(), "previous\n");
  EXPECT_TRUE(source.isConnected());
  write("recovered");
  EXPECT_EQ(source.getContent(), "recovered\n");
  EXPECT_EQ(source.previous(), "recovered\n");
  EXPECT_EQ(source.failures(), 0);
}

TEST_F(FileSourceTests, LogsOversizeOnceUntilAValidRead) {
  InspectableFile source(path.c_str(), 0);
  writeRepeated(maxContentSize + 1, '\n');
  supla_test_clear_last_log();
  source.getContent();
  EXPECT_NE(std::string(supla_test_get_last_log()).find("exceeds 10 MiB"),
            std::string::npos);
  supla_test_clear_last_log();
  for (int i = 0; i < 6; ++i) {
    source.getContent();
    EXPECT_TRUE(std::string(supla_test_get_last_log()).empty());
  }
  write("valid\n");
  ASSERT_EQ(source.getContent(), "valid\n");
  writeRepeated(maxContentSize + 1, '\n');
  supla_test_clear_last_log();
  source.getContent();
  EXPECT_NE(std::string(supla_test_get_last_log()).find("exceeds 10 MiB"),
            std::string::npos);
}

TEST_F(FileSourceTests, RealIoErrorIsNotAcceptedAsEndOfFile) {
  InspectableFile source(path.c_str(), 0);
  write("previous");
  ASSERT_EQ(source.getContent(), "previous\n");
  ASSERT_TRUE(std::filesystem::remove(path));
  ASSERT_TRUE(std::filesystem::create_directory(path));
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(source.getContent(), "previous\n");
  }
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_TRUE(source.previous().empty());
  EXPECT_EQ(source.failures(), 0);
}

TEST_F(FileSourceTests, ExpiredFilesStillBypassTheReadFailureCounter) {
  InspectableFile source(path.c_str(), 1);
  write("previous");
  ASSERT_EQ(source.getContent(), "previous\n");
  std::filesystem::last_write_time(
      path, std::filesystem::file_time_type::clock::now() -
                std::chrono::seconds(10));
  EXPECT_FALSE(source.isConnected());
  EXPECT_TRUE(source.getContent().empty());
  EXPECT_EQ(source.failures(), 0);
  EXPECT_EQ(source.previous(), "previous\n");
  source.setExpirationTime(0);
  EXPECT_EQ(source.getContent(), "previous\n");
}
