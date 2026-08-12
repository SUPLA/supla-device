// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <openssl/rand.h>
#include <supla/log_wrapper.h>
#include <supla/tools.h>

#include <cstdlib>

void deviceSoftwareReset() {
  std::exit(1);
}

bool isDeviceSoftwareResetSupported() {
  return true;
}

bool isLastResetSoft() {
  // TODO(klew): implement
  return false;
}

bool Supla::isLastResetPower() {
  // TODO(klew): implement
  return false;
}

int Supla::getPlatformId() {
  // TODO(klew): do we need platfom id for linux SW?
  return 0;
}

void Supla::fillRandom(uint8_t *buffer, int size) {
  if (buffer == nullptr || size <= 0) {
    SUPLA_LOG_ERROR("fillRandom: invalid buffer or size");
    std::exit(1);
  }

  if (RAND_bytes(buffer, size) != 1) {
    SUPLA_LOG_ERROR("fillRandom: OpenSSL RAND_bytes failed");
    std::exit(1);
  }
}
