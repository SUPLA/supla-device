/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifndef SRC_SUPLA_SUPLET_DEFINITION_CACHE_H_
#define SRC_SUPLA_SUPLET_DEFINITION_CACHE_H_

#include <stddef.h>
#include <stdint.h>
#include <supla/suplet/config.h>

namespace Supla {

class Config;

namespace Suplet {

class Sha256Provider {
 public:
  virtual ~Sha256Provider() {
  }
  virtual bool calculate(const uint8_t *data,
                         size_t dataSize,
                         uint8_t *output,
                         size_t outputSize) = 0;
};

#if !defined(SUPLA_TEST) && (defined(ESP32) || defined(SUPLA_DEVICE_ESP32))
class DefaultSha256Provider : public Sha256Provider {
 public:
  bool calculate(const uint8_t *data,
                 size_t dataSize,
                 uint8_t *output,
                 size_t outputSize) override;
};
#endif

struct CachedDefinitionInfo {
  uint32_t definitionId = 0;
  uint16_t definitionVersion = 0;
  uint16_t jsonSize = 0;
  uint8_t sha256[32] = {};
};

struct DefinitionCacheHandle {
  uint8_t slot = 255;
  uint8_t variant = 0;
};

class DefinitionCache {
 public:
  DefinitionCache(Supla::Config *config, Sha256Provider *sha256Provider);

  bool save(uint32_t definitionId,
            uint16_t definitionVersion,
            const char *json,
            const uint8_t *expectedSha256);
  bool load(uint32_t definitionId,
            uint16_t definitionVersion,
            char *json,
            size_t jsonSize,
            CachedDefinitionInfo *info = nullptr) const;
  bool contains(uint32_t definitionId, uint16_t definitionVersion) const;
  bool erase(uint32_t definitionId, uint16_t definitionVersion);
  bool getInfo(uint8_t index, CachedDefinitionInfo *info) const;
  bool beginStagedSave(uint32_t definitionId,
                       uint16_t definitionVersion,
                       uint16_t jsonSize,
                       const uint8_t *sha256,
                       DefinitionCacheHandle *handle);
  bool writeStagedChunk(DefinitionCacheHandle handle,
                        uint16_t chunkIndex,
                        const uint8_t *data,
                        uint16_t size);
  bool loadStaged(DefinitionCacheHandle handle,
                  uint32_t definitionId,
                  uint16_t definitionVersion,
                  char *json,
                  size_t jsonSize,
                  CachedDefinitionInfo *info = nullptr) const;
  bool commitStaged(DefinitionCacheHandle handle,
                    uint32_t definitionId,
                    uint16_t definitionVersion,
                    uint16_t jsonSize,
                    const uint8_t *sha256);
  bool abortStaged(DefinitionCacheHandle handle);

 private:
  bool calculateAndVerify(const char *json,
                          uint16_t jsonSize,
                          const uint8_t *expectedSha256,
                          uint8_t *calculatedSha256) const;
  bool loadActiveHeader(uint8_t index,
                        CachedDefinitionInfo *info,
                        uint8_t *activeVariant = nullptr,
                        uint16_t *chunkCount = nullptr,
                        uint16_t *chunkSize = nullptr) const;
  bool loadHeader(uint8_t index,
                  uint8_t variant,
                  CachedDefinitionInfo *info,
                  uint16_t *chunkCount = nullptr,
                  uint16_t *chunkSize = nullptr) const;
  bool saveSlot(uint8_t index,
                uint32_t definitionId,
                uint16_t definitionVersion,
                const char *json,
                uint16_t jsonSize,
                const uint8_t *sha256);
  bool saveHeader(uint8_t index,
                  uint8_t variant,
                  const CachedDefinitionInfo &info,
                  uint16_t chunkCount,
                  uint16_t chunkSize);
  bool loadPayload(uint8_t slot,
                   uint8_t variant,
                   const CachedDefinitionInfo &info,
                   uint16_t chunkCount,
                   uint16_t chunkSize,
                   char *json,
                   size_t jsonSize) const;
  bool eraseChunks(uint8_t index, uint8_t variant);
  bool eraseVariant(uint8_t index, uint8_t variant);
  bool eraseLegacySlot(uint8_t index);
  bool eraseSlot(uint8_t index);
  bool setActiveVariant(uint8_t index, uint8_t variant);
  bool variantExists(uint8_t index, uint8_t variant) const;
  bool slotExists(uint8_t index) const;
  int findSlot(uint32_t definitionId, uint16_t definitionVersion) const;
  int findFreeSlot() const;
  static uint8_t otherVariant(uint8_t variant);
  static bool isValidVariant(uint8_t variant);
  static bool isValidHandle(DefinitionCacheHandle handle);
  static bool makeActKey(uint8_t index, char *output, size_t outputSize);
  static bool makeHeaderKey(uint8_t index,
                            uint8_t variant,
                            char *output,
                            size_t outputSize);
  static bool makeChunkKey(uint8_t index,
                           uint8_t variant,
                           uint16_t chunkIndex,
                           char *output,
                           size_t outputSize);
  static bool makeLegacyHeaderKey(uint8_t index,
                                  char *output,
                                  size_t outputSize);
  static bool makeLegacyChunkKey(uint8_t index,
                                 uint16_t chunkIndex,
                                 char *output,
                                 size_t outputSize);

  Supla::Config *config = nullptr;
  Sha256Provider *sha256Provider = nullptr;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_DEFINITION_CACHE_H_
