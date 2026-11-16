// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_LOOP_PROFILER_H_
#define SRC_SUPLA_LOOP_PROFILER_H_

// Diagnostic timing of SuplaDevice.iterate(). Disabled builds keep no code or
// state. Enable with -DSUPLA_LOOP_PROFILING_ENABLED=1 for the whole build.
#ifndef SUPLA_LOOP_PROFILING_ENABLED
#define SUPLA_LOOP_PROFILING_ENABLED 0
#endif

#if SUPLA_LOOP_PROFILING_ENABLED

#include <stdint.h>

namespace Supla {
namespace LoopProfiler {

enum Stage : uint8_t {
  kHousekeeping,       // config save, mode checks, local action triggers
  kElementsAlways,     // Element::iterateAlways() of every element
  kChannelHandles,     // ChannelHandle::iterateAll()
  kStateStorage,       // periodic state save
  kNetwork,            // network setup and Network::Iterate()
  kProtocols,          // ProtocolLayer::iterate() of every protocol
  kElementsConnected,  // Element::iterateConnected()
  kSwUpdate,           // OTA check and update
  kStageCount
};

// Items are timed inside a stage to name the slowest ones.
enum ItemKind : uint8_t {
  kItemAlways,
  kItemConnected,
  kItemProtocol,
  kItemKindCount
};

uint32_t nowUs();
void stage(Stage next);
// index is the position on the element/protocol list; channel may be -1.
void item(ItemKind kind, uint16_t index, int channel, uint32_t startUs);

// Covers one SuplaDevice.iterate() call, including its early returns. Time
// between two scopes is reported as "outside": the application's own loop.
struct Scope {
  Scope();
  ~Scope();
};

}  // namespace LoopProfiler
}  // namespace Supla

#define SUPLA_LOOP_STAGE(name) \
  Supla::LoopProfiler::stage(Supla::LoopProfiler::name)

#else

#define SUPLA_LOOP_STAGE(name) do {} while (0)

#endif  // SUPLA_LOOP_PROFILING_ENABLED

#endif  // SRC_SUPLA_LOOP_PROFILER_H_
