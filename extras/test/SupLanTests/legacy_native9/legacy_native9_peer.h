// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_SUPLANTESTS_LEGACY_NATIVE9_LEGACY_NATIVE9_PEER_H_
#define EXTRAS_TEST_SUPLANTESTS_LEGACY_NATIVE9_LEGACY_NATIVE9_PEER_H_
#include <stddef.h>
#include <stdint.h>
// C boundary between separately compiled current and frozen legacy namespaces.
extern "C" {
bool legacyPeerInit();
void legacyPeerReceive(const uint8_t *data, size_t length);
void legacyPeerIterate(uint32_t now);
size_t legacyPeerTransmit(uint8_t *data, size_t capacity);
uint32_t legacyPeerControlCalls();
uint32_t legacyPeerInvalidData();
}
#endif  // EXTRAS_TEST_SUPLANTESTS_LEGACY_NATIVE9_LEGACY_NATIVE9_PEER_H_
