// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

  static std::string commandUsingPercentPlaceholder(
      std::string_view placeholder, const std::filesystem::path& outputPath) {
    return "printf \"$(printf '\\045s')\" " + std::string(placeholder) + " > " +
           outputPath.string();
  }

  std::filesystem::path tempDirectory;
};

TEST_F(Sd4linuxCmdOutputTests, ExactSingleQuoteInjectionPocStaysData) {
  const auto outputPath = filePath("output.txt");
  const auto markerPath = filePath("injected-marker");
  auto output = makeOutput("printf '%s' '{}' > " + outputPath.string());
  const std::string payload = "; touch " + markerPath.string() + "; #";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests,
       DoubleQuotedPlaceholderKeepsShellSyntaxAsPayload) {
  const auto outputPath = filePath("double-quoted-output.txt");
  const auto markerPath = filePath("double-quoted-marker");
  auto output = makeOutput("printf '%s' \"{}\" > " + outputPath.string());
  const std::string payload = "$(touch " + markerPath.string() + ")\n`touch " +
                              markerPath.string() + "`\n\"; touch " +
                              markerPath.string() +
                              "; #\n$HOME\nbackslash\\\nnewline";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests, UnquotedPlaceholderStillWorks) {
  const auto outputPath = filePath("unquoted-output.txt");
  auto output = makeOutput("printf '%s' {} > " + outputPath.string());
  const std::string payload = "unquoted payload";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
}

TEST_F(Sd4linuxCmdOutputTests, FullSpecialCharacterPayloadStaysData) {
  const auto outputPath = filePath("special-output.txt");
  const auto markerPath = filePath("special-marker");
  auto output = makeOutput("printf '%s' {} > " + outputPath.string());
  const std::string payload = "single' double\" backslash\\ ; | > < $(touch " +
                              markerPath.string() + ") `touch " +
                              markerPath.string() + "` $HOME\nnewline";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests, PassesEmptyStringAsOneEmptyArgument) {
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

TEST_F(Sd4linuxCmdOutputTests, EmptyVectorProducesNoArguments) {
  const auto outputPath = filePath("empty-vector-output.txt");
  auto output = makeOutput("printf '[%s]' {} > " + outputPath.string());

  ASSERT_TRUE(output->putContent(std::vector<int>()));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "[]");
}

TEST_F(Sd4linuxCmdOutputTests, NoPlaceholderAppendsScalarArgumentSafely) {
  const auto outputPath = filePath("fallback-output.txt");
  auto output = makeOutput("printf '%s' > " + outputPath.string());
  const std::string payload = "100% complete; not shell syntax";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
}

TEST_F(Sd4linuxCmdOutputTests, RejectsNulPayloadBeforeExecutingCommand) {
  const auto markerPath = filePath("nul-marker");
  auto output = makeOutput("touch " + markerPath.string());
  std::string payload = "before";
  payload.push_back('\0');
  payload += "after";

  EXPECT_FALSE(output->putContent(payload));
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

TEST_F(Sd4linuxCmdOutputTests,
       SelectsPlaceholdersInBracesPercentSPercentDOrder) {
  const auto outputPath = filePath("placeholder-order-output.txt");
  auto output =
      makeOutput("printf \"$(printf '\\045s\\045s\\045s')\" %s %d {} > " +
                 outputPath.string());
  const std::string payload = "value";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "%s%dvalue");
}

TEST_F(Sd4linuxCmdOutputTests, ReplacesOnlyTheFirstMatchingPlaceholder) {
  const auto outputPath = filePath("first-placeholder-output.txt");
  auto output = makeOutput("printf \"$(printf '\\045s\\045s')\" {} {} > " +
                           outputPath.string());
  const std::string payload = "value";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "value{}");
}

TEST_F(Sd4linuxCmdOutputTests, SupportsPercentSPlaceholder) {
  const auto outputPath = filePath("percent-s-output.txt");
  auto output = makeOutput(commandUsingPercentPlaceholder("%s", outputPath));
  const std::string payload = "percent-s payload";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
}

TEST_F(Sd4linuxCmdOutputTests, SupportsPercentDPlaceholder) {
  const auto outputPath = filePath("percent-d-output.txt");
  auto output = makeOutput(commandUsingPercentPlaceholder("%d", outputPath));
  const std::string payload = "percent-d payload";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), payload);
}

TEST_F(Sd4linuxCmdOutputTests, RetainsTrustedRedirection) {
  const auto outputPath = filePath("redirected-output.txt");
  auto output = makeOutput("printf '%s' {} > " + outputPath.string());

  ASSERT_TRUE(output->putContent(123));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "123");
}

TEST_F(Sd4linuxCmdOutputTests, RetainsTrustedPipeAndAndAndCommandSubstitution) {
  const auto outputPath = filePath("trusted-shell-output.txt");
  auto output = makeOutput(
      "home=\"${HOME}\"; prefix=\"$(printf trusted)\" && printf "
      "'%s:%s' \"$prefix\" {} > " +
      outputPath.string());
  const std::string payload = "trusted payload";

  ASSERT_TRUE(output->putContent(payload));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "trusted:trusted payload");

  const auto pipeOutputPath = filePath("pipe-output.txt");
  auto pipeOutput =
      makeOutput("printf '%s' {} | tr a-z A-Z > " + pipeOutputPath.string());
  ASSERT_TRUE(pipeOutput->putContent(std::string("pipe payload")));
  ASSERT_TRUE(std::filesystem::exists(pipeOutputPath));
  EXPECT_EQ(readFile(pipeOutputPath), "PIPE PAYLOAD");
}

TEST_F(Sd4linuxCmdOutputTests, PassesBooleanTextValue) {
  const auto outputPath = filePath("boolean-output.txt");
  auto output = makeOutput("printf '%s' {} > " + outputPath.string());

  ASSERT_TRUE(output->putContent(true));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "true");

  ASSERT_TRUE(output->putContent(false));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_EQ(readFile(outputPath), "false");
}

TEST_F(Sd4linuxCmdOutputTests, NonZeroCommandExitReturnsFalse) {
  auto output = makeOutput("false");

  EXPECT_FALSE(output->putContent(1));
}

TEST_F(Sd4linuxCmdOutputTests,
       RejectsPlaceholderAfterBackslashWithoutExecutingCommand) {
  const auto markerPath = filePath("unsupported-marker");
  const auto outputPath = filePath("unsupported-output.txt");
  auto output = makeOutput("touch " + markerPath.string() +
                           " && printf '%s' \\{} > " + outputPath.string());

  EXPECT_FALSE(output->putContent(std::string("payload")));
  EXPECT_FALSE(std::filesystem::exists(markerPath));
  EXPECT_FALSE(std::filesystem::exists(outputPath));
}

TEST_F(Sd4linuxCmdOutputTests,
       RejectsPlaceholderInArithmeticExpansionWithoutExecutingCommand) {
  const auto markerPath = filePath("arithmetic-marker");
  const auto outputPath = filePath("arithmetic-output.txt");
  auto output = makeOutput("touch " + markerPath.string() +
                           " && printf '%s' $(({})) > " + outputPath.string());

  EXPECT_FALSE(output->putContent(std::string("payload")));
  EXPECT_FALSE(std::filesystem::exists(markerPath));
  EXPECT_FALSE(std::filesystem::exists(outputPath));
}

TEST_F(Sd4linuxCmdOutputTests,
       RejectsNoPlaceholderFallbackWithCommentWithoutExecutingCommand) {
  const auto markerPath = filePath("comment-marker");
  auto output =
      makeOutput("touch " + markerPath.string() + " && printf '%s' # comment");

  EXPECT_FALSE(output->putContent(std::string("payload")));
  EXPECT_FALSE(std::filesystem::exists(markerPath));
}

}  // namespace
