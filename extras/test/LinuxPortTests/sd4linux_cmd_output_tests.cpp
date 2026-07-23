/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtest/gtest.h>
#include <supla/output/cmd.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

class Sd4linuxCmdOutputTests : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto directoryTemplate =
        std::filesystem::temp_directory_path() /
        ("supla_cmd_output_tests_" + std::to_string(getpid()) + "_XXXXXX");
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

  std::filesystem::path filePath(std::string_view name) const {
    return tempDirectory / std::string(name);
  }

  std::unique_ptr<Supla::Output::Output> makeOutput(
      const std::string& command) const {
    return std::make_unique<Supla::Output::Cmd>(command);
  }

  static std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
  }

  static std::string injectionPayload(const std::filesystem::path& markerPath) {
    const auto marker = markerPath.string();
    return "payload with spaces; touch " + marker + "; ' $(touch " + marker +
           ") `touch " + marker + "` | >\n touch " + marker;
  }

  static std::string commandUsingPercentPlaceholder(
      std::string_view placeholder, const std::filesystem::path& outputPath) {
    return "value=" + std::string(placeholder) +
           "; printf \"$(printf '\\045s')\" \"$value\" > " +
           outputPath.string();
  }

  std::filesystem::path tempDirectory;
};

TEST_F(Sd4linuxCmdOutputTests, WritesBenignBracesPayloadWithoutChangingIt) {
  const auto outputPath = filePath("output.txt");
  auto output = makeOutput("printf '%s' {} > " + outputPath.string());
  const std::string payload = "ordinary payload";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
}

TEST_F(Sd4linuxCmdOutputTests,
       QuotesShellSyntaxInBracesPayloadAndDoesNotCreateMarker) {
  const auto outputPath = filePath("output.txt");
  const auto markerPath = filePath("injected-marker");
  auto output = makeOutput("printf '%s' {} > " + outputPath.string());
  const auto payload = injectionPayload(markerPath);

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests, QuotesPayloadUsedByPercentSPlaceholder) {
  const auto outputPath = filePath("percent-s-output.txt");
  const auto markerPath = filePath("percent-s-marker");
  auto output = makeOutput(commandUsingPercentPlaceholder("%s", outputPath));
  const auto payload = injectionPayload(markerPath);

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests, QuotesStringPayloadUsedByPercentDPlaceholder) {
  const auto outputPath = filePath("percent-d-output.txt");
  const auto markerPath = filePath("percent-d-marker");
  auto output = makeOutput(commandUsingPercentPlaceholder("%d", outputPath));
  const auto payload = injectionPayload(markerPath);

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests,
       AppendsPayloadWithoutPlaceholderAndDoesNotCreateMarker) {
  const auto outputPath = filePath("fallback-output.txt");
  const auto markerPath = filePath("fallback-marker");
  auto output =
      makeOutput("printf \"$(printf '\\045s')\" > " + outputPath.string());
  const auto payload = injectionPayload(markerPath);

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests, PassesEmptyStringAsAnEmptyArgument) {
  const auto outputPath = filePath("empty-output.txt");
  auto output = makeOutput("printf '[%s]' {} > " + outputPath.string());

  ASSERT_TRUE(output->putContent(std::string()));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "[]");
}

TEST_F(Sd4linuxCmdOutputTests, KeepsVectorElementsAsSeparateArguments) {
  const auto outputPath = filePath("vector-output.txt");
  auto output = makeOutput("printf '<%s><%s><%s>' {} > " + outputPath.string());

  ASSERT_TRUE(output->putContent(std::vector<int>{1, -2, 3}));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "<1><-2><3>");
}

TEST_F(Sd4linuxCmdOutputTests, RejectsNulPayloadWithoutExecutingCommand) {
  const auto markerPath = filePath("nul-marker");
  auto output = makeOutput("touch " + markerPath.string());
  std::string payload = "before";
  payload.push_back('\0');
  payload += "after";

  EXPECT_FALSE(output->putContent(payload));
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests,
       RetainsTrustedShellRedirectionInCommandTemplate) {
  const auto outputPath = filePath("redirected-output.txt");
  auto output = makeOutput("printf '%s' {} > " + outputPath.string());

  ASSERT_TRUE(output->putContent(123));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "123");
}

TEST_F(Sd4linuxCmdOutputTests, PreservesBooleanTextValue) {
  const auto outputPath = filePath("boolean-output.txt");
  auto output = makeOutput("printf '%s' {} > " + outputPath.string());

  ASSERT_TRUE(output->putContent(true));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "true");

  ASSERT_TRUE(output->putContent(false));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "false");
}

TEST_F(Sd4linuxCmdOutputTests, ReplacesOnlyTheFirstMatchingPlaceholder) {
  const auto outputPath = filePath("first-placeholder-output.txt");
  auto output = makeOutput("printf '%s|%s' {} {} > " + outputPath.string());
  const std::string payload = "value";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "value|{}");
}

}  // namespace
