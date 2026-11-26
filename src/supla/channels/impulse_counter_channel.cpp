// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "impulse_counter_channel.h"

using Supla::ImpulseCounterChannel;

double ImpulseCounterChannel::calculate(uint64_t count) const {
  const uint32_t rate = config.ImpulsesPerUnit;
  if (rate == 0) {
    return 0;
  }

  // Apply the offset in whole impulses. Integer truncation can differ from
  // Core at a floating-point boundary. The clamped product fits in uint64_t.
  constexpr uint64_t maxInitialValue = 100000000000ULL;
  const auto initialValue = static_cast<uint64_t>(config.InitialValue);
  const uint64_t magnitude = config.InitialValue < 0
      ? uint64_t{0} - initialValue
      : initialValue;
  const uint64_t offset =
      magnitude < maxInitialValue ? magnitude : maxInitialValue;
  const uint64_t offsetImpulses = offset * rate / 1000;
  if (config.InitialValue < 0) {
    count -= count < offsetImpulses ? count : offsetImpulses;
  } else {
    const uint64_t available = UINT64_MAX - count;
    count += available < offsetImpulses ? available : offsetImpulses;
  }
  const auto remainder = static_cast<uint32_t>(count % rate);
  return static_cast<double>(count / rate) +
         static_cast<double>(remainder * uint32_t{1000} / rate) / 1000;
}

bool ImpulseCounterChannel::getImpulseCounterValue(double *value,
                                                   const char **unit) {
  if (value) {
    *value = calculate(getValueInt64());
  }
  if (unit) {
    *unit = config.CustomUnit;
  }
  return value == nullptr || config.ImpulsesPerUnit > 0;
}
