// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_EXAMPLES_LINUX_SLEEPY_THERMOMETER_H_
#define EXTRAS_EXAMPLES_LINUX_SLEEPY_THERMOMETER_H_

#include <cstdint>

namespace Supla {
class LinuxSupLanRuntime;

class SleepyThermometer {
 public:
  SleepyThermometer(uint64_t cycle, uint32_t readyMs, uint16_t awakeMs,
                    uint32_t tailMs, uint64_t nextStartMs);
  bool initialize();
  bool iterate(LinuxSupLanRuntime& runtime);
  int finish(bool saved, bool stopped) const;

 private:
  void log(const char* event, const char* details = "") const;
  void progress(const char* event, uint32_t count, uint32_t& seen) const;

  const uint64_t cycle_;
  const uint64_t startMs_;
  const uint64_t nextStartMs_;
  const uint32_t readyMs_;
  const uint32_t tailMs_;
  const uint16_t awakeMs_;
  uint64_t wakeMs_ = 0;
  uint64_t sleepMs_ = 0;
  uint32_t resets_ = 0;
  uint32_t awakeHints_ = 0;
  uint32_t reads_ = 0;
  uint32_t states_ = 0;
  uint32_t stateBaseline_ = 0;
  uint32_t retries_ = 0;
  uint8_t channelNumber_ = 0;
  bool udpSeen_ = false;
  bool serverSeen_ = false;
  bool failed_ = false;
};
}  // namespace Supla

#endif  // EXTRAS_EXAMPLES_LINUX_SLEEPY_THERMOMETER_H_
