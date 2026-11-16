// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "loop_profiler.h"

#if SUPLA_LOOP_PROFILING_ENABLED

#include <supla/log_wrapper.h>

#include <inttypes.h>

#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#elif defined(ARDUINO)
#include <Arduino.h>
#else
#include <chrono>  // NOLINT(build/c++11)
#endif

namespace Supla {
namespace LoopProfiler {
namespace {

const uint32_t kWindowUs = 10000000;
const uint16_t kMaxItems = 128;

struct Timing {
  uint32_t sumUs = 0;
  uint32_t maxUs = 0;
  void add(uint32_t us) {
    sumUs += us;
    if (us > maxUs) maxUs = us;
  }
};

struct Item {
  uint32_t sumUs;
  uint32_t maxUs;
  int16_t channel;
};

uint32_t windowStartUs = 0;
uint32_t scopeStartUs = 0;
uint32_t stageStartUs = 0;
uint32_t lastScopeStartUs = 0;
uint32_t lastScopeEndUs = 0;
uint32_t calls = 0;
bool havePrevious = false;
Stage currentStage = kHousekeeping;
Timing period, run, outside;
Timing stages[kStageCount];
Item items[kItemKindCount][kMaxItems] = {};

// Prints tenths of a millisecond without floating point.
#define SUPLA_LOOP_MS "%" PRIu32 ".%" PRIu32
#define SUPLA_LOOP_MS_ARG(us) (us) / 1000, ((us) % 1000) / 100

void reportItems(ItemKind kind, const char *name) {
  // The three items with the largest total time in this window.
  for (uint8_t rank = 0; rank < 3; ++rank) {
    int best = -1;
    for (uint16_t i = 0; i < kMaxItems; ++i) {
      if (items[kind][i].sumUs &&
          (best < 0 || items[kind][i].sumUs > items[kind][best].sumUs))
        best = i;
    }
    if (best < 0) break;
    const Item &item = items[kind][best];
    SUPLA_LOG_INFO("SD loop slow %s #%d channel=%d ms avg/max="
                   SUPLA_LOOP_MS "/" SUPLA_LOOP_MS,
                   name, best, static_cast<int>(item.channel),
                   SUPLA_LOOP_MS_ARG(item.sumUs / calls),
                   SUPLA_LOOP_MS_ARG(item.maxUs));
    items[kind][best].sumUs = 0;  // Skip it when ranking the next one.
  }
}

void report() {
  const uint32_t periods = calls;
  SUPLA_LOG_INFO("SD loop calls=%" PRIu32 " ms avg/max period=" SUPLA_LOOP_MS
                 "/" SUPLA_LOOP_MS " run=" SUPLA_LOOP_MS "/" SUPLA_LOOP_MS
                 " outside=" SUPLA_LOOP_MS "/" SUPLA_LOOP_MS,
                 calls, SUPLA_LOOP_MS_ARG(period.sumUs / periods),
                 SUPLA_LOOP_MS_ARG(period.maxUs),
                 SUPLA_LOOP_MS_ARG(run.sumUs / calls),
                 SUPLA_LOOP_MS_ARG(run.maxUs),
                 SUPLA_LOOP_MS_ARG(outside.sumUs / periods),
                 SUPLA_LOOP_MS_ARG(outside.maxUs));
  static const char *const names[kStageCount] = {
      "housekeeping", "elements_always", "channel_handles", "state_storage",
      "network", "protocols", "elements_connected", "sw_update"};
  // Stage averages count skipped stages as zero; maxima are per call.
  for (uint8_t i = 0; i < kStageCount; i += 2) {
    SUPLA_LOG_INFO("SD loop stages ms avg/max %s=" SUPLA_LOOP_MS "/"
                   SUPLA_LOOP_MS " %s=" SUPLA_LOOP_MS "/" SUPLA_LOOP_MS,
                   names[i], SUPLA_LOOP_MS_ARG(stages[i].sumUs / calls),
                   SUPLA_LOOP_MS_ARG(stages[i].maxUs), names[i + 1],
                   SUPLA_LOOP_MS_ARG(stages[i + 1].sumUs / calls),
                   SUPLA_LOOP_MS_ARG(stages[i + 1].maxUs));
  }
  reportItems(kItemAlways, "always");
  reportItems(kItemConnected, "connected");
  reportItems(kItemProtocol, "protocol");
  calls = 0;
  period = run = outside = Timing();
  for (auto &timing : stages) timing = Timing();
  for (auto &kind : items)
    for (auto &item : kind) item = Item();
}

}  // namespace

uint32_t nowUs() {
#if defined(ESP_PLATFORM)
  return static_cast<uint32_t>(esp_timer_get_time());
#elif defined(ARDUINO)
  return micros();
#else
  // Host builds and unit tests: independent of the mocked millis().
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}

void stage(Stage next) {
  const uint32_t now = nowUs();
  stages[currentStage].add(now - stageStartUs);
  stageStartUs = now;
  currentStage = next;
}

void item(ItemKind kind, uint16_t index, int channel, uint32_t startUs) {
  if (index >= kMaxItems) index = kMaxItems - 1;  // Overflow shares a slot.
  Item &entry = items[kind][index];
  const uint32_t elapsed = nowUs() - startUs;
  entry.sumUs += elapsed;
  if (elapsed > entry.maxUs) entry.maxUs = elapsed;
  entry.channel = static_cast<int16_t>(channel);
}

Scope::Scope() {
  scopeStartUs = stageStartUs = nowUs();
  currentStage = kHousekeeping;
  if (!calls) windowStartUs = scopeStartUs;
  if (havePrevious) {
    period.add(scopeStartUs - lastScopeStartUs);
    outside.add(scopeStartUs - lastScopeEndUs);
  }
  lastScopeStartUs = scopeStartUs;
  havePrevious = true;
}

Scope::~Scope() {
  const uint32_t now = nowUs();
  // Accumulate per-call stage totals before taking their maxima.
  stages[currentStage].add(now - stageStartUs);
  run.add(now - scopeStartUs);
  ++calls;
  if (now - windowStartUs >= kWindowUs) report();
  // Reporting is excluded from the next "outside" interval; the start-to-start
  // period of that one call still contains it.
  lastScopeEndUs = nowUs();
}

}  // namespace LoopProfiler
}  // namespace Supla

#endif  // SUPLA_LOOP_PROFILING_ENABLED
