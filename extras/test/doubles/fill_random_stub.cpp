// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>
#include <supla/tools.h>

#include <cstdlib>
#include <fstream>

void Supla::fillRandom(uint8_t *buffer, int size) {
  if (buffer == nullptr || size <= 0) {
    SUPLA_LOG_ERROR("fillRandom: invalid buffer or size");
    std::exit(1);
  }

  std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
  if (!urandom.is_open()) {
    SUPLA_LOG_ERROR("fillRandom: failed to open /dev/urandom");
    std::exit(1);
  }

  urandom.read(reinterpret_cast<char *>(buffer), size);
  if (urandom.gcount() != size) {
    SUPLA_LOG_ERROR("fillRandom: failed to read random bytes");
    std::exit(1);
  }
}
