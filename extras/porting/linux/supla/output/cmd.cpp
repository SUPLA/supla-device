// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cmd.h"

#include <supla/log_wrapper.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class ShellQuoteContext {
  kUnquoted,
  kSingleQuoted,
  kDoubleQuoted,
};

enum class PayloadKind {
  kScalar,
  kVector,
};

struct Placeholder {
  size_t position;
  std::string_view text;
};

class ShellTemplateScanner {
 public:
  ShellTemplateScanner(std::string_view command, size_t targetPosition)
      : command(command), targetPosition(targetPosition) {
  }

  std::optional<ShellQuoteContext> findTargetContext() {
    size_t position = 0;
    if (!scanShell(position, '\0') || !targetFound || unsupportedTarget) {
      return std::nullopt;
    }
    return targetContext;
  }

  bool validate() {
    size_t position = 0;
    return scanShell(position, '\0');
  }

  bool containsComment() const {
    return hasComment;
  }

 private:
  bool isTarget(size_t position) const {
    return targetPosition != std::string_view::npos &&
           position == targetPosition;
  }

  void markTarget(ShellQuoteContext context) {
    if (!targetFound) {
      targetFound = true;
      targetContext = context;
    }
    if (parameterExpansionDepth > 0) {
      unsupportedTarget = true;
    }
  }

  void markUnsupportedTarget() {
    if (targetPosition != std::string_view::npos) {
      targetFound = true;
      unsupportedTarget = true;
    }
  }

  bool scanParameterExpansion(size_t& position) {
    ++parameterExpansionDepth;
    int braceDepth = 1;

    while (position < command.size()) {
      if (isTarget(position)) {
        markUnsupportedTarget();
      }

      if (command[position] == '\\') {
        if (position + 1 >= command.size()) {
          --parameterExpansionDepth;
          return false;
        }
        if (isTarget(position + 1)) {
          markUnsupportedTarget();
        }
        position += 2;
        continue;
      }

      if (command.compare(position, 2, "${") == 0) {
        ++braceDepth;
        position += 2;
        continue;
      }

      if (command.compare(position, 3, "$((") == 0) {
        --parameterExpansionDepth;
        return false;
      }

      if (command.compare(position, 2, "$(") == 0) {
        position += 2;
        if (!scanShell(position, ')')) {
          --parameterExpansionDepth;
          return false;
        }
        continue;
      }

      if (command[position] == '`') {
        ++position;
        if (!scanShell(position, '`')) {
          --parameterExpansionDepth;
          return false;
        }
        continue;
      }

      if (command[position] == '\'' || command[position] == '"') {
        --parameterExpansionDepth;
        return false;
      }

      if (command[position] == '}') {
        --braceDepth;
        ++position;
        if (braceDepth == 0) {
          --parameterExpansionDepth;
          return true;
        }
        continue;
      }

      ++position;
    }

    --parameterExpansionDepth;
    return false;
  }

  bool scanComment(size_t& position) {
    hasComment = true;
    const auto commentStart = position;
    while (position < command.size() && command[position] != '\n') {
      ++position;
    }

    if (targetPosition != std::string_view::npos &&
        targetPosition >= commentStart && targetPosition < position) {
      markUnsupportedTarget();
    }
    return true;
  }

  bool scanShell(size_t& position, char terminator) {
    ShellQuoteContext quoteContext = ShellQuoteContext::kUnquoted;
    bool wordStarted = false;

    while (position < command.size()) {
      const char character = command[position];

      if (quoteContext == ShellQuoteContext::kSingleQuoted) {
        if (isTarget(position)) {
          markTarget(quoteContext);
        }
        ++position;
        if (character == '\'') {
          quoteContext = ShellQuoteContext::kUnquoted;
        }
        continue;
      }

      if (quoteContext == ShellQuoteContext::kDoubleQuoted) {
        if (isTarget(position)) {
          markTarget(quoteContext);
        }

        if (character == '"') {
          quoteContext = ShellQuoteContext::kUnquoted;
          ++position;
          continue;
        }

        if (character == '\\') {
          if (position + 1 >= command.size()) {
            return false;
          }
          if (isTarget(position + 1)) {
            markUnsupportedTarget();
          }
          position += 2;
          continue;
        }

        if (command.compare(position, 3, "$((") == 0) {
          return false;
        }

        if (command.compare(position, 2, "$(") == 0) {
          position += 2;
          if (!scanShell(position, ')')) {
            return false;
          }
          continue;
        }

        if (character == '`') {
          ++position;
          if (!scanShell(position, '`')) {
            return false;
          }
          continue;
        }

        if (command.compare(position, 2, "${") == 0) {
          position += 2;
          if (!scanParameterExpansion(position)) {
            return false;
          }
          continue;
        }

        ++position;
        continue;
      }

      if (terminator != '\0' && character == terminator) {
        ++position;
        return true;
      }

      if (isTarget(position)) {
        markTarget(quoteContext);
      }

      if (character == '\\') {
        if (position + 1 >= command.size()) {
          return false;
        }
        if (isTarget(position + 1)) {
          markUnsupportedTarget();
        }
        position += 2;
        wordStarted = true;
        continue;
      }

      if (character == '\'') {
        ++position;
        quoteContext = ShellQuoteContext::kSingleQuoted;
        wordStarted = true;
        continue;
      }

      if (character == '"') {
        ++position;
        quoteContext = ShellQuoteContext::kDoubleQuoted;
        wordStarted = true;
        continue;
      }

      if (command.compare(position, 3, "$((") == 0) {
        return false;
      }

      if (command.compare(position, 2, "$(") == 0) {
        position += 2;
        if (!scanShell(position, ')')) {
          return false;
        }
        wordStarted = true;
        continue;
      }

      if (character == '`') {
        ++position;
        if (!scanShell(position, '`')) {
          return false;
        }
        wordStarted = true;
        continue;
      }

      if (command.compare(position, 2, "${") == 0) {
        position += 2;
        if (!scanParameterExpansion(position)) {
          return false;
        }
        wordStarted = true;
        continue;
      }

      if (command.compare(position, 2, "<<") == 0) {
        return false;
      }

      if (character == '#' && !wordStarted) {
        if (!scanComment(position)) {
          return false;
        }
        continue;
      }

      if (character == '\n' || character == ' ' || character == '\t' ||
          character == '\r' || character == ';' || character == '|' ||
          character == '&' || character == '>' || character == '<' ||
          character == '(' || character == ')') {
        ++position;
        wordStarted = false;
        continue;
      }

      ++position;
      wordStarted = true;
    }

    return quoteContext == ShellQuoteContext::kUnquoted && terminator == '\0';
  }

  std::string_view command;
  size_t targetPosition;
  size_t parameterExpansionDepth = 0;
  bool targetFound = false;
  bool unsupportedTarget = false;
  bool hasComment = false;
  ShellQuoteContext targetContext = ShellQuoteContext::kUnquoted;
};

static bool isPrintfFormatSpecifier(std::string_view command, size_t position) {
  if (position == 0) {
    return false;
  }

  for (const char quote : {'\'', '"'}) {
    const auto openingQuote = command.rfind(quote, position - 1);
    if (openingQuote == std::string_view::npos) {
      continue;
    }

    size_t commandEnd = openingQuote;
    while (commandEnd > 0 &&
           std::isspace(static_cast<unsigned char>(command[commandEnd - 1]))) {
      --commandEnd;
    }

    constexpr std::string_view printfName = "printf";
    if (commandEnd < printfName.size() ||
        command.substr(commandEnd - printfName.size(), printfName.size()) !=
            printfName) {
      continue;
    }

    if (commandEnd > printfName.size()) {
      const auto previous = command[commandEnd - printfName.size() - 1];
      if (std::isalnum(static_cast<unsigned char>(previous)) ||
          previous == '_') {
        continue;
      }
    }

    return true;
  }

  return false;
}

static std::optional<Placeholder> findPlaceholder(
    std::string_view trustedCmdTemplate) {
  constexpr std::string_view placeholders[] = {"{}", "%s", "%d"};

  for (const auto placeholder : placeholders) {
    size_t position = 0;
    while ((position = trustedCmdTemplate.find(placeholder, position)) !=
           std::string_view::npos) {
      if ((placeholder == "%s" || placeholder == "%d") &&
          isPrintfFormatSpecifier(trustedCmdTemplate, position)) {
        position += placeholder.size();
        continue;
      }
      return Placeholder{position, placeholder};
    }
  }
  return std::nullopt;
}

static std::string placeholderReplacement(ShellQuoteContext context,
                                          PayloadKind payloadKind) {
  const auto parameter = payloadKind == PayloadKind::kScalar ? "$1" : "$@";

  switch (context) {
    case ShellQuoteContext::kUnquoted:
      return std::string("\"") + parameter + "\"";
    case ShellQuoteContext::kDoubleQuoted:
      return parameter;
    case ShellQuoteContext::kSingleQuoted:
      return std::string("'\"") + parameter + "\"'";
  }

  return {};
}

static std::optional<std::string> buildCmd(std::string_view trustedCmdTemplate,
                                           PayloadKind payloadKind) {
  const auto placeholder = findPlaceholder(trustedCmdTemplate);
  if (!placeholder) {
    ShellTemplateScanner scanner(trustedCmdTemplate, std::string_view::npos);
    if (!scanner.validate() || scanner.containsComment()) {
      return std::nullopt;
    }

    std::string command(trustedCmdTemplate);
    if (!command.empty() && command.back() != ' ') {
      command.push_back(' ');
    }
    command += payloadKind == PayloadKind::kScalar ? "\"$1\"" : "\"$@\"";
    return command;
  }

  ShellTemplateScanner scanner(trustedCmdTemplate, placeholder->position);
  const auto context = scanner.findTargetContext();
  if (!context) {
    return std::nullopt;
  }

  std::string command(trustedCmdTemplate);
  command.replace(placeholder->position,
                  placeholder->text.size(),
                  placeholderReplacement(*context, payloadKind));
  return command;
}

static bool execCmd(const std::string& transformedTemplate,
                    const std::vector<std::string>& payloadArguments) {
  std::vector<std::string> argvStorage;
  argvStorage.reserve(payloadArguments.size() + 4);
  argvStorage.emplace_back("sh");
  argvStorage.emplace_back("-c");
  argvStorage.emplace_back(transformedTemplate);
  argvStorage.emplace_back("supla-cmd");
  for (const auto& argument : payloadArguments) {
    argvStorage.push_back(argument);
  }

  std::vector<char*> argv;
  argv.reserve(argvStorage.size() + 1);
  for (auto& argument : argvStorage) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);

  SUPLA_LOG_DEBUG("Command template: %s, argument count: %zu",
                  transformedTemplate.c_str(),
                  payloadArguments.size());

  const pid_t pid = fork();
  if (pid == -1) {
    SUPLA_LOG_WARNING("Failed to fork command process: %s", strerror(errno));
    return false;
  }

  if (pid == 0) {
    execv("/bin/sh", argv.data());
    _exit(127);
  }

  int status = 0;
  pid_t result;
  do {
    result = waitpid(pid, &status, 0);
  } while (result == -1 && errno == EINTR);

  if (result == -1) {
    SUPLA_LOG_WARNING("Failed to wait for command process: %s",
                      strerror(errno));
    return false;
  }

  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool putScalarContent(std::string_view trustedCmdTemplate,
                             std::string payload) {
  if (payload.find('\0') != std::string::npos) {
    return false;
  }

  const auto command = buildCmd(trustedCmdTemplate, PayloadKind::kScalar);
  if (!command) {
    return false;
  }

  return execCmd(*command, {std::move(payload)});
}

Supla::Output::Cmd::Cmd(std::string cmd) : cmdLine(cmd) {
}

Supla::Output::Cmd::~Cmd() {
}

bool Supla::Output::Cmd::putContent(int payload) {
  if (cmdLine.empty()) return false;
  return putScalarContent(cmdLine, std::to_string(payload));
}

bool Supla::Output::Cmd::putContent(bool payload) {
  if (cmdLine.empty()) return false;
  return putScalarContent(cmdLine, payload ? "true" : "false");
}

bool Supla::Output::Cmd::putContent(const std::string& payload) {
  if (cmdLine.empty()) return false;
  return putScalarContent(cmdLine, payload);
}

bool Supla::Output::Cmd::putContent(const std::vector<int>& payload) {
  if (cmdLine.empty()) return false;

  const auto command = buildCmd(cmdLine, PayloadKind::kVector);
  if (!command) {
    return false;
  }

  std::vector<std::string> arguments;
  arguments.reserve(payload.size());
  for (const int value : payload) {
    arguments.push_back(std::to_string(value));
  }

  return execCmd(*command, arguments);
}
