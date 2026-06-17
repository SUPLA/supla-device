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

#include <gtest/gtest.h>

#include <stdint.h>
#include <string.h>

#include <supla/sha256.h>

TEST(Sha256Tests, TestImplementationMatchesSingleAndChunkedUpdate) {
  const uint8_t input[] = {'a', 'b', 'c', 'd', 'e'};
  uint8_t single[32] = {};
  uint8_t chunked[32] = {};

  Supla::Sha256 singleSha;
  singleSha.update(input, sizeof(input));
  singleSha.digest(single);

  Supla::Sha256 chunkedSha;
  chunkedSha.update(input, 2);
  chunkedSha.update(input + 2, sizeof(input) - 2);
  chunkedSha.digest(chunked);

  EXPECT_EQ(memcmp(single, chunked, sizeof(single)), 0);
}

TEST(Sha256Tests, TestImplementationReturnsRequestedPrefix) {
  const uint8_t input[] = {'a', 'b', 'c', 'd', 'e'};
  uint8_t full[32] = {};
  uint8_t prefix[16] = {};

  Supla::Sha256 fullSha;
  fullSha.update(input, sizeof(input));
  fullSha.digest(full);

  Supla::Sha256 prefixSha;
  prefixSha.update(input, sizeof(input));
  prefixSha.digest(prefix, sizeof(prefix));

  EXPECT_EQ(memcmp(full, prefix, sizeof(prefix)), 0);
}

TEST(Sha256Tests, TestImplementationIgnoresInvalidInput) {
  uint8_t output[32] = {};
  uint8_t expected[32] = {};

  Supla::Sha256 sha;
  sha.update(nullptr, 4);
  sha.update(reinterpret_cast<const uint8_t *>("abc"), 0);
  sha.digest(nullptr);
  sha.digest(output);

  EXPECT_EQ(memcmp(output, expected, sizeof(output)), 0);
}
