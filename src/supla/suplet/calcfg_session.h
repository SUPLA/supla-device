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

#ifndef SRC_SUPLA_SUPLET_CALCFG_SESSION_H_
#define SRC_SUPLA_SUPLET_CALCFG_SESSION_H_

#include <stdint.h>

#include <supla/sha256.h>
#include <supla/suplet/config.h>
#include <supla/suplet/definition_cache.h>
#include <supla/suplet/storage.h>

namespace Supla {
namespace Suplet {

struct InstanceCalcfgSession {
  bool active = false;
  uint32_t sessionId = 0;
  uint32_t lastActivityMs = 0;
  uint8_t instanceId = 0;
  bool upgrade = false;
  uint32_t definitionId = 0;
  uint16_t fromDefinitionVersion = 0;
  uint16_t definitionVersion = 0;
  uint16_t paramsSize = 0;
  uint16_t receivedSize = 0;
  uint8_t expectedSha256[32] = {};
  uint8_t params[SUPLA_SUPLET_MAX_CONFIG_SIZE + 1] = {};
  uint8_t received[SUPLA_SUPLET_MAX_CONFIG_SIZE] = {};
};

struct DefinitionCalcfgSession {
  bool active = false;
  uint32_t sessionId = 0;
  uint32_t lastActivityMs = 0;
  uint32_t definitionId = 0;
  uint16_t definitionVersion = 0;
  uint16_t jsonSize = 0;
  uint16_t receivedSize = 0;
  DefinitionCacheHandle cacheHandle = {};
  uint16_t currentChunkIndex = 0;
  uint16_t currentChunkSize = 0;
  uint8_t expectedSha256[32] = {};
  uint8_t currentChunk[SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE] = {};
  Supla::Sha256 sha256 = {};
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_CALCFG_SESSION_H_
