// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <suplan_runtime_linux.h>

#include <cstdio>
#include <string>

// Required by the actual Linux logger linked with the SDK.
int logLevel = 0;
int runAsDaemon = 0;

namespace {
class Writer : public Supla::Debug::ResponseWriter {
 public:
  void write(const char *text) override { output += text; }
  std::string output;
};
}  // namespace

int main() {
  Supla::LinuxSupLanConfig config;
  config.enabled = true;
  Supla::LinuxSupLanRuntime runtime(config);
  if (!runtime.initialize()) return 1;
  Writer writer;
  const char *mutations[] = {
      "local-valve-open", "local-channel-function", "sleep-hint", "wake-hint",
      "hvac-upload", "transport", "forget-session", "clear-endpoint",
      "fault", "mtu", "set-resource-value", "emit-action"};
  for (const auto *command : mutations) {
    writer.output.clear();
    // Missing arguments exercise command recognition without sending packets
    // or changing a real channel/configuration, including in the test build.
    const bool handled = runtime.processDebugCommand(command, &writer);
#ifdef SUPLAN_TESTING
    if (!handled || writer.output.empty()) {
#else
    if (handled || !writer.output.empty()) {
#endif
      std::fprintf(stderr, "Unexpected command availability: %s\n", command);
      return 2;
    }
  }
  const char *diagnostics[] = {"suplan-help", "show-status", "show-resources",
      "show-counters", "show-pools", "remote-state", "channel-state",
      "hvac-config", "read", "control"};
  for (const auto *command : diagnostics) {
    if (!runtime.processDebugCommand(command, &writer)) {
      std::fprintf(stderr, "Missing normal command: %s\n", command);
      return 3;
    }
  }
  writer.output.clear();
  runtime.processDebugCommand("suplan-help", &writer);
  for (const auto *command : mutations) {
#ifdef SUPLAN_TESTING
    if (writer.output.find(command) == std::string::npos) {
#else
    if (writer.output.find(command) != std::string::npos) {
#endif
      std::fprintf(stderr, "Unexpected help availability: %s\n", command);
      return 4;
    }
  }
  std::puts("PASS: 12 mutation commands, 10 normal commands, help isolation");
  return 0;
}
