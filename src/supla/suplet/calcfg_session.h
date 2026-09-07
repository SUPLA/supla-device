// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_CALCFG_SESSION_H_
#define SRC_SUPLA_SUPLET_CALCFG_SESSION_H_

#include <stdint.h>

#include <supla/suplet/config.h>
#include <supla/suplet/definition_cache.h>
#include <supla/suplet/storage.h>

namespace Supla {
namespace Suplet {

#if SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE > \
    SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE
#define SUPLA_SUPLET_TRANSFER_STORAGE_CHUNK_SIZE \
  SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE
#else
#define SUPLA_SUPLET_TRANSFER_STORAGE_CHUNK_SIZE \
  SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE
#endif

enum class CalcfgTransferType : uint8_t {
  None = 0,
  Definition = 1,
  Instance = 2,
};

enum class ArtifactUpdateMode : uint8_t {
  Replace = 0,
  Remove = 1,
  Keep = 2,
};

struct CalcfgSession {
  bool active = false;
  CalcfgTransferType type = CalcfgTransferType::None;
  uint32_t lastActivityMs = 0;

  uint32_t definitionId = 0;
  uint16_t definitionVersion = 0;
  uint16_t definitionSize = 0;
  uint16_t definitionReceivedSize = 0;
  DefinitionCacheHandle definitionCacheHandle = {};

  uint8_t instanceId = 0;
  uint32_t revision = 0;
  uint16_t configSize = 0;
  uint16_t configReceivedSize = 0;
  uint32_t artifactSize = 0;
  uint32_t artifactReceivedSize = 0;
  ArtifactUpdateMode artifactUpdateMode = ArtifactUpdateMode::Remove;
  ArtifactStorageHandle artifactStorageHandle = {};
  uint8_t config[SUPLA_SUPLET_MAX_CONFIG_SIZE + 1] = {};

  uint16_t storageChunkIndex = 0;
  uint16_t storageChunkSize = 0;
  uint8_t storageChunk[SUPLA_SUPLET_TRANSFER_STORAGE_CHUNK_SIZE] = {};
};

#undef SUPLA_SUPLET_TRANSFER_STORAGE_CHUNK_SIZE

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_CALCFG_SESSION_H_
