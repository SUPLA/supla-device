// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_INPUT_NOISE_GUARD_H_
#define SRC_SUPLA_INPUT_NOISE_GUARD_H_

#include <stdint.h>

#ifndef SUPLA_INPUT_NOISE_GUARD_WIFI_TRANSITION_MS
#define SUPLA_INPUT_NOISE_GUARD_WIFI_TRANSITION_MS 1000
#endif

namespace Supla {
namespace InputNoiseGuard {

void NotifyWifiTransition();
void NotifyWifiStaDisconnected();
void NotifyWifiStaConnected();
void IgnoreForMs(uint32_t timeoutMs);

void SetWifiTransitionGuardMs(uint32_t timeoutMs);
uint32_t GetWifiTransitionGuardMs();

bool IsActive();
uint32_t RemainingMs();
void Clear();

}  // namespace InputNoiseGuard
}  // namespace Supla

#endif  // SRC_SUPLA_INPUT_NOISE_GUARD_H_
