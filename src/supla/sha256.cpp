/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef SUPLA_TEST

#include "sha256.h"

Supla::Sha256::Sha256() {
  sha256_init(&hash);
}

Supla::Sha256::~Sha256() {
}

void Supla::Sha256::update(const uint8_t *data, const int size) {
  sha256_update(&hash, size, static_cast<const unsigned char *>(data));
}

struct sha256_ctx* Supla::Sha256::getHash() {
  return &hash;
}

void Supla::Sha256::digest(uint8_t *output, int length) {
  sha256_digest(&hash, length, output);
}

#endif  // SUPLA_TEST

