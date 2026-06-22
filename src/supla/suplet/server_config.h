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

#ifndef SRC_SUPLA_SUPLET_SERVER_CONFIG_H_
#define SRC_SUPLA_SUPLET_SERVER_CONFIG_H_

#include <stddef.h>
#include <stdint.h>
#include <supla/suplet/assignment_applier.h>
#include <supla/suplet/definition_cache.h>
#include <supla/suplet/json_definition.h>

namespace Supla {
namespace Suplet {

enum class ServerConfigResult : uint8_t {
  Applied = 0,
  Removed = 1,
  InvalidArgument = 2,
  DefinitionNotSupported = 3,
  InvalidDefinition = 4,
  InvalidConfig = 5,
  StorageError = 6,
  ResourceLimitExceeded = 7,
  CreateOnlyParamChanged = 8,
  Busy = 9,
  TopologyChangeNotAllowed = 10,
  DefinitionNotFound = 11,
  DefinitionCannotBeChanged = 12,
  InstanceLimitExceeded = 13,
  ChannelLimitExceeded = 14,
};

struct CachedDefinitionDetails {
  CachedDefinitionInfo cache = {};
  Category category = Category::Unknown;
  Kind kind = Kind::Unknown;
  uint8_t schemaVersion = 0;
  uint8_t handlerVersion = 0;
  uint8_t maxInstances = 0;
};

class DownloadedDefinitionStore {
 public:
  bool load(const DefinitionCache &cache,
            uint32_t definitionId,
            uint16_t definitionVersion,
            JsonDefinition *definition,
            CachedDefinitionInfo *info = nullptr) const;
  uint8_t getCount(const DefinitionCache &cache) const;
};

class ServerConfigHandler {
 public:
  ServerConfigHandler(
      Manager *manager,
      Registry *registry,
      DefinitionCache *definitionCache = nullptr,
      DownloadedDefinitionStore *downloadedDefinitions = nullptr);

  ServerConfigResult loadDownloadedDefinitions();
  ServerConfigResult saveDownloadedDefinition(uint32_t definitionId,
                                              uint16_t definitionVersion,
                                              const char *definitionJson,
                                              const uint8_t *sha256);
  ServerConfigResult removeDownloadedDefinition(uint32_t definitionId,
                                                uint16_t definitionVersion);
  uint8_t getCachedDefinitionCount() const;
  bool getCachedDefinitionDetails(uint8_t listIndex,
                                  CachedDefinitionDetails *details) const;
  ServerConfigResult garbageCollectUnusedDefinitions();
  ServerConfigResult applyAssignmentJson(const char *assignmentJson,
                                         uint32_t definitionId,
                                         uint16_t definitionVersion);
  ServerConfigResult applyInstanceParams(uint8_t instanceId,
                                         uint32_t definitionId,
                                         uint16_t definitionVersion,
                                         const char *paramsJson,
                                         uint16_t paramsSize,
                                         uint8_t *appliedInstanceId = nullptr);
  ServerConfigResult validateAssignmentJson(
      const char *assignmentJson,
      uint32_t definitionId,
      uint16_t definitionVersion) const;
  ServerConfigResult validateInstanceParams(
      uint8_t instanceId,
      uint32_t definitionId,
      uint16_t definitionVersion,
      const char *paramsJson,
      uint16_t paramsSize) const;
  ServerConfigResult applyCommandJson(const char *commandJson);
  ServerConfigResult validateCommandJson(const char *commandJson) const;
  ServerConfigResult removeAssignment(uint8_t instanceId);
  bool loadDownloadedDefinition(uint32_t definitionId,
                                uint16_t definitionVersion,
                                JsonDefinition *definition,
                                CachedDefinitionInfo *info = nullptr) const;

  bool isRuntimeRefreshRequired() const;
  void clearRuntimeRefreshRequired();

 private:
  static ServerConfigResult fromAssignmentResult(AssignmentResult result);

  Manager *manager = nullptr;
  Registry *registry = nullptr;
  DefinitionCache *definitionCache = nullptr;
  DownloadedDefinitionStore *downloadedDefinitions = nullptr;
  bool runtimeRefreshRequired = false;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_SERVER_CONFIG_H_
