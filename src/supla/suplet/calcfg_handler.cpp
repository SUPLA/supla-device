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

#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <stddef.h>
#include <string.h>
#include <supla/sha256.h>
#include <supla/suplet/calcfg_session.h>
#include <supla/suplet/capability_registry.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/server_config.h>
#include <supla/time.h>

namespace {

uint8_t supletDetailFromServerResult(Supla::Suplet::ServerConfigResult result) {
  switch (result) {
    case Supla::Suplet::ServerConfigResult::Applied:
    case Supla::Suplet::ServerConfigResult::Removed:
      return SUPLA_CALCFG_SUPLET_RESULT_OK;
    case Supla::Suplet::ServerConfigResult::InvalidArgument:
      return SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST;
    case Supla::Suplet::ServerConfigResult::DefinitionNotSupported:
      return SUPLA_CALCFG_SUPLET_RESULT_UNSUPPORTED_DEFINITION;
    case Supla::Suplet::ServerConfigResult::DefinitionCannotBeChanged:
      return SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_CANNOT_BE_CHANGED;
    case Supla::Suplet::ServerConfigResult::InvalidDefinition:
      return SUPLA_CALCFG_SUPLET_RESULT_INVALID_DEFINITION;
    case Supla::Suplet::ServerConfigResult::DefinitionNotFound:
      return SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_NOT_FOUND;
    case Supla::Suplet::ServerConfigResult::InvalidConfig:
      return SUPLA_CALCFG_SUPLET_RESULT_INVALID_CONFIG;
    case Supla::Suplet::ServerConfigResult::ResourceLimitExceeded:
    case Supla::Suplet::ServerConfigResult::InstanceLimitExceeded:
      return SUPLA_CALCFG_SUPLET_RESULT_INSTANCE_LIMIT_EXCEEDED;
    case Supla::Suplet::ServerConfigResult::ChannelLimitExceeded:
      return SUPLA_CALCFG_SUPLET_RESULT_CHANNEL_LIMIT_EXCEEDED;
    case Supla::Suplet::ServerConfigResult::CreateOnlyParamChanged:
      return SUPLA_CALCFG_SUPLET_RESULT_CREATE_ONLY_PARAM_CHANGED;
    case Supla::Suplet::ServerConfigResult::TopologyChangeNotAllowed:
      return SUPLA_CALCFG_SUPLET_RESULT_TOPOLOGY_CHANGE_NOT_ALLOWED;
    case Supla::Suplet::ServerConfigResult::Busy:
      return SUPLA_CALCFG_SUPLET_RESULT_BUSY;
    case Supla::Suplet::ServerConfigResult::StorageError:
    default:
      return SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR;
  }
}

Supla::Suplet::ServerConfigResult flushDefinitionCalcfgChunk(
    Supla::Suplet::ServerConfigHandler *handler,
    Supla::Suplet::DefinitionCalcfgSession *session) {
  if (handler == nullptr || session == nullptr) {
    return Supla::Suplet::ServerConfigResult::InvalidArgument;
  }
  if (session->currentChunkSize == 0) {
    return Supla::Suplet::ServerConfigResult::Applied;
  }
  auto result = handler->writeStagedDownloadedDefinitionChunk(
      session->cacheHandle,
      session->currentChunkIndex,
      session->currentChunk,
      session->currentChunkSize);
  if (result != Supla::Suplet::ServerConfigResult::Applied) {
    return result;
  }
  session->currentChunkIndex++;
  session->currentChunkSize = 0;
  return Supla::Suplet::ServerConfigResult::Applied;
}

int calcfgResultFromServerResult(Supla::Suplet::ServerConfigResult result) {
  switch (result) {
    case Supla::Suplet::ServerConfigResult::Applied:
    case Supla::Suplet::ServerConfigResult::Removed:
      return SUPLA_CALCFG_RESULT_DONE;
    case Supla::Suplet::ServerConfigResult::DefinitionNotFound:
      return SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
    case Supla::Suplet::ServerConfigResult::DefinitionNotSupported:
      return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
    default:
      return SUPLA_CALCFG_RESULT_FALSE;
  }
}

void fillSupletResult(TDS_DeviceCalCfgResult *result,
                      uint8_t detailCode,
                      uint8_t phase,
                      uint8_t instanceId = 0,
                      uint32_t definitionId = 0,
                      uint16_t definitionVersion = 0,
                      uint16_t required = 0,
                      uint16_t available = 0) {
  if (result == nullptr) {
    return;
  }
  TCalCfg_SupletResult payload = {};
  payload.Version = 1;
  payload.DetailCode = detailCode;
  payload.Phase = phase;
  payload.InstanceId = instanceId;
  payload.DefinitionId = definitionId;
  payload.DefinitionVersion = definitionVersion;
  payload.Required = required;
  payload.Available = available;
  memcpy(result->Data, &payload, sizeof(payload));
  result->DataSize = sizeof(payload);
}

void calculateSha256(const uint8_t *data, uint16_t size, uint8_t *output) {
  if (output == nullptr) {
    return;
  }
  Supla::Sha256 sha256;
  if (size > 0 && data != nullptr) {
    sha256.update(data, size);
  }
  sha256.digest(output, 32);
}

uint16_t getBuiltinDefinitionJsonSize(
    const Supla::Suplet::Definition *definition) {
  if (definition == nullptr || definition->definitionJson == nullptr) {
    return 0;
  }
  if (definition->definitionJsonSize != 0) {
    return definition->definitionJsonSize;
  }
  size_t size = strlen(definition->definitionJson);
  return size > UINT16_MAX ? 0 : static_cast<uint16_t>(size);
}

}  // namespace

namespace Supla {
namespace Suplet {

int Manager::handleCalcfg(TSD_DeviceCalCfgRequest *request,
                          TDS_DeviceCalCfgResult *result) {
  if (request == nullptr || result == nullptr || !isServerConfigReady()) {
    fillSupletResult(result,
                     SUPLA_CALCFG_SUPLET_RESULT_UNSUPPORTED_DEFINITION,
                     SUPLA_CALCFG_SUPLET_PHASE_NONE);
    return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
  }

  const uint32_t nowMs = millis();
  cleanupExpiredCalcfgSessions(nowMs);

  auto supletRegistry = getRegistry();
  auto supletServerConfigHandler = getServerConfigHandler();
  auto supletCalcfgSession = getInstanceCalcfgSession();
  auto supletDefinitionCalcfgSession = getDefinitionCalcfgSession();
  auto table = getInstanceTable();
  if (table == nullptr) {
    fillSupletResult(result,
                     SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR,
                     SUPLA_CALCFG_SUPLET_PHASE_NONE);
    return SUPLA_CALCFG_RESULT_FALSE;
  }

  switch (request->Command) {
    case SUPLA_CALCFG_CMD_SUPLET_GET_CAPABILITIES: {
      auto supletCapabilityRegistry = getCapabilityRegistry();
      if (supletCapabilityRegistry == nullptr) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_UNSUPPORTED_DEFINITION,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
      }

      TCalCfg_SupletListRequest listRequest = {};
      listRequest.Limit = SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS;
      if (request->DataSize != 0) {
        if (request->DataSize != sizeof(listRequest)) {
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE);
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        memcpy(&listRequest, request->Data, sizeof(listRequest));
      }

      TCalCfg_SupletCapabilityList output = {};
      output.Offset = listRequest.Offset;
      uint8_t total = supletCapabilityRegistry->getCount();
      output.Total = total;
      uint8_t limit = listRequest.Limit;
      if (limit == 0 || limit > SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS) {
        limit = SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS;
      }
      for (uint8_t i = 0; i < limit && listRequest.Offset + i < total; i++) {
        Supla::Suplet::Capability capability = {};
        if (!supletCapabilityRegistry->getCapability(listRequest.Offset + i,
                                                     &capability)) {
          break;
        }
        auto &item = output.Items[output.Count++];
        item.Category = static_cast<uint8_t>(capability.category);
        item.Kind = static_cast<uint8_t>(capability.kind);
        item.MinSchemaVersion = capability.minSchemaVersion;
        item.MaxSchemaVersion = capability.maxSchemaVersion;
        item.HandlerVersion = capability.handlerVersion;
        item.MaxInstances = capability.maxInstances;
        item.SupportsDownloadedDefinition =
            capability.supportsDownloadedDefinition;
        item.DefinitionId = capability.definitionId;
        item.MinDefinitionVersion = capability.minDefinitionVersion;
        item.MaxDefinitionVersion = capability.maxDefinitionVersion;
      }
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = sizeof(output);
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_LIST: {
      TCalCfg_SupletListRequest listRequest = {};
      listRequest.Limit = SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS;
      if (request->DataSize != 0) {
        if (request->DataSize != sizeof(listRequest)) {
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE);
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        memcpy(&listRequest, request->Data, sizeof(listRequest));
      }

      TCalCfg_SupletDefinitionList output = {};
      output.Offset = listRequest.Offset;
      const uint8_t builtinCount = supletRegistry->getCount();
      const uint8_t cachedCount =
          supletServerConfigHandler->getCachedDefinitionCount();
      const uint16_t combinedTotal = builtinCount + cachedCount;
      const uint8_t total = combinedTotal > UINT8_MAX
                                ? UINT8_MAX
                                : static_cast<uint8_t>(combinedTotal);
      output.Total = total;
      uint8_t limit = listRequest.Limit;
      if (limit == 0 || limit > SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS) {
        limit = SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS;
      }
      for (uint8_t i = 0; i < limit && listRequest.Offset + i < total; i++) {
        const uint8_t listIndex = listRequest.Offset + i;
        auto &item = output.Items[output.Count++];
        if (listIndex < builtinCount) {
          Supla::Suplet::Capability capability = {};
          if (!supletRegistry->getCapability(listIndex, &capability)) {
            output.Count--;
            break;
          }
          const auto *definition = supletRegistry->findDefinition(
              capability.definitionId, capability.minDefinitionVersion);
          item.Category = static_cast<uint8_t>(capability.category);
          item.Kind = static_cast<uint8_t>(capability.kind);
          item.SchemaVersion = capability.minSchemaVersion;
          item.HandlerVersion = capability.handlerVersion;
          item.MaxInstances = capability.maxInstances;
          item.Source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_BUILTIN;
          item.DefinitionId = capability.definitionId;
          item.DefinitionVersion = capability.minDefinitionVersion;
          item.JsonSize = getBuiltinDefinitionJsonSize(definition);
          if (definition != nullptr && item.JsonSize > 0) {
            calculateSha256(
                reinterpret_cast<const uint8_t *>(definition->definitionJson),
                item.JsonSize,
                item.JsonSha256);
          }
        } else {
          Supla::Suplet::CachedDefinitionDetails details = {};
          if (!supletServerConfigHandler->getCachedDefinitionDetails(
                  listIndex - builtinCount, &details)) {
            output.Count--;
            break;
          }
          item.Category = static_cast<uint8_t>(details.category);
          item.Kind = static_cast<uint8_t>(details.kind);
          item.SchemaVersion = details.schemaVersion;
          item.HandlerVersion = details.handlerVersion;
          item.MaxInstances = details.maxInstances;
          item.Source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_CACHED;
          item.DefinitionId = details.cache.definitionId;
          item.DefinitionVersion = details.cache.definitionVersion;
          item.JsonSize = details.cache.jsonSize;
          memcpy(item.JsonSha256,
                 details.cache.sha256,
                 sizeof(item.JsonSha256));
        }
      }
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = sizeof(output);
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_CONFIG: {
      if (request->DataSize !=
          sizeof(TCalCfg_SupletDefinitionConfigRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletDefinitionConfigRequest configRequest = {};
      memcpy(&configRequest, request->Data, sizeof(configRequest));
      if (configRequest.DefinitionId == 0 ||
          configRequest.DefinitionVersion == 0) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }

      const char *builtinJson = nullptr;
      char *cachedJson = nullptr;
      uint16_t totalSize = 0;
      uint8_t source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_BUILTIN;
      const auto *definition = supletRegistry->findDefinition(
          configRequest.DefinitionId, configRequest.DefinitionVersion);
      if (definition != nullptr) {
        builtinJson = definition->definitionJson;
        totalSize = getBuiltinDefinitionJsonSize(definition);
        if (builtinJson == nullptr || totalSize == 0) {
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_NOT_FOUND,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE);
          return SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
        }
      } else {
        source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_CACHED;
        Supla::Suplet::CachedDefinitionInfo info = {};
        cachedJson = new char[SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1];
        if (cachedJson == nullptr) {
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_RAM_LIMIT_EXCEEDED,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE);
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        if (!supletServerConfigHandler->loadDownloadedDefinitionJson(
                configRequest.DefinitionId,
                configRequest.DefinitionVersion,
                cachedJson,
                SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1,
                &info)) {
          delete[] cachedJson;
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_NOT_FOUND,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE);
          return SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
        }
        totalSize = info.jsonSize;
      }

      if (configRequest.Offset >= totalSize) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE,
                         0,
                         configRequest.DefinitionId,
                         configRequest.DefinitionVersion,
                         totalSize,
                         configRequest.Offset);
        delete[] cachedJson;
        return SUPLA_CALCFG_RESULT_FALSE;
      }

      uint8_t maxSize = configRequest.MaxSize;
      if (maxSize == 0 ||
          maxSize > SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE) {
        maxSize = SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE;
      }
      const uint16_t remaining = totalSize - configRequest.Offset;
      const uint8_t chunkSize =
          remaining > maxSize ? maxSize : static_cast<uint8_t>(remaining);

      TCalCfg_SupletDefinitionConfigChunk output = {};
      output.DefinitionId = configRequest.DefinitionId;
      output.DefinitionVersion = configRequest.DefinitionVersion;
      output.Offset = configRequest.Offset;
      output.TotalSize = totalSize;
      output.Source = source;
      output.Size = chunkSize;
      if (source == SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_BUILTIN) {
        memcpy(output.Data, builtinJson + configRequest.Offset, chunkSize);
      } else {
        memcpy(output.Data, cachedJson + configRequest.Offset, chunkSize);
      }
      delete[] cachedJson;
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize =
          offsetof(TCalCfg_SupletDefinitionConfigChunk, Data) + output.Size;
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_COUNT: {
      if (request->DataSize != 0) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceCount output = {};
      output.Count = table->getCount();
      output.MaxInstances = SUPLA_SUPLET_MAX_INSTANCES;
      output.MaxChannelsPerInstance = SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE;
      output.MaxCachedDefinitions = SUPLA_SUPLET_MAX_CACHED_DEFINITIONS;
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = sizeof(output);
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_LIST: {
      if (request->DataSize != sizeof(TCalCfg_SupletListRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletListRequest listRequest = {};
      memcpy(&listRequest, request->Data, sizeof(listRequest));
      TCalCfg_SupletInstanceList output = {};
      output.Offset = listRequest.Offset;
      output.Total = table->getCount();
      uint8_t limit = listRequest.Limit;
      if (limit == 0 || limit > SUPLA_CALCFG_SUPLET_INSTANCE_LIST_MAX_ITEMS) {
        limit = SUPLA_CALCFG_SUPLET_INSTANCE_LIST_MAX_ITEMS;
      }
      for (uint8_t i = 0;
           i < limit && listRequest.Offset + i < table->getCount();
           i++) {
        auto record = table->getRecord(listRequest.Offset + i);
        if (record == nullptr) {
          break;
        }
        auto &item = output.Items[output.Count++];
        item.InstanceId = record->instanceId;
        item.DefinitionId = record->definitionId;
        item.DefinitionVersion = record->definitionVersion;
        item.SubDeviceId = record->subDeviceId;
        uint8_t channelCount = 0;
        const auto *definition = supletRegistry->findDefinition(
            record->definitionId, record->definitionVersion);
        if (definition != nullptr) {
          channelCount = definition->channelCount;
        }
        item.ChannelCount = channelCount;
      }
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = sizeof(output);
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_INFO: {
      if (request->DataSize != sizeof(TCalCfg_SupletInstanceRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceRequest instanceRequest = {};
      memcpy(&instanceRequest, request->Data, sizeof(instanceRequest));
      auto record = table->findByInstanceId(instanceRequest.InstanceId);
      if (record == nullptr) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_NOT_FOUND,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE,
                         instanceRequest.InstanceId);
        return SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
      }

      TCalCfg_SupletInstanceInfo output = {};
      output.InstanceId = record->instanceId;
      output.DefinitionId = record->definitionId;
      output.DefinitionVersion = record->definitionVersion;
      output.SubDeviceId = record->subDeviceId;
      output.ParamsSize = record->configSize;
      Supla::Suplet::InstanceRecord fullRecord = {};
      const Supla::Suplet::InstanceRecord *configRecord = record;
      if (record->config == nullptr && record->configSize > 0) {
        if (!loadInstance(record->instanceId, &fullRecord)) {
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE,
                           record->instanceId);
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        configRecord = &fullRecord;
      }
      calculateSha256(
          configRecord->config, configRecord->configSize, output.ParamsSha256);
      const auto *definition = supletRegistry->findDefinition(
          record->definitionId, record->definitionVersion);
      if (definition != nullptr) {
        output.ChannelCount = definition->channelCount;
      }
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = sizeof(output);
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_CONFIG: {
      if (request->DataSize != sizeof(TCalCfg_SupletInstanceConfigRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceConfigRequest configRequest = {};
      memcpy(&configRequest, request->Data, sizeof(configRequest));
      auto record = table->findByInstanceId(configRequest.InstanceId);
      if (record == nullptr) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_NOT_FOUND,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE,
                         configRequest.InstanceId);
        return SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
      }
      Supla::Suplet::InstanceRecord fullRecord = {};
      const Supla::Suplet::InstanceRecord *configRecord = record;
      if (record->config == nullptr && record->configSize > 0) {
        if (!loadInstance(record->instanceId, &fullRecord)) {
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE,
                           record->instanceId);
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        configRecord = &fullRecord;
      }
      if (configRequest.Offset > record->configSize) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE,
                         record->instanceId);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceConfigChunk output = {};
      output.InstanceId = record->instanceId;
      output.Offset = configRequest.Offset;
      output.TotalSize = record->configSize;
      uint16_t left = record->configSize - configRequest.Offset;
      uint8_t maxSize = configRequest.MaxSize;
      if (maxSize == 0 || maxSize > SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE) {
        maxSize = SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE;
      }
      output.Size = left > maxSize ? maxSize : static_cast<uint8_t>(left);
      if (output.Size > 0) {
        memcpy(output.Data,
               configRecord->config + configRequest.Offset,
               output.Size);
      }
      result->DataSize =
          offsetof(TCalCfg_SupletInstanceConfigChunk, Data) + output.Size;
      memcpy(result->Data, &output, result->DataSize);
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN: {
      if (request->DataSize != sizeof(TCalCfg_SupletDefinitionBegin)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      if (supletDefinitionCalcfgSession != nullptr &&
          supletDefinitionCalcfgSession->active) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_BUSY,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletDefinitionBegin begin = {};
      memcpy(&begin, request->Data, sizeof(begin));
      if (begin.SessionId == 0 || begin.DefinitionId == 0 ||
          begin.DefinitionVersion == 0 || begin.JsonSize == 0 ||
          begin.JsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE,
                         0,
                         begin.DefinitionId,
                         begin.DefinitionVersion,
                         begin.JsonSize,
                         SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      Supla::Suplet::DefinitionCacheHandle cacheHandle = {};
      auto serverResult =
          supletServerConfigHandler->beginStagedDownloadedDefinition(
              begin.DefinitionId,
              begin.DefinitionVersion,
              begin.JsonSize,
              begin.JsonSha256,
              &cacheHandle);
      if (serverResult != Supla::Suplet::ServerConfigResult::Applied) {
        fillSupletResult(result,
                         supletDetailFromServerResult(serverResult),
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE,
                         0,
                         begin.DefinitionId,
                         begin.DefinitionVersion,
                         begin.JsonSize,
                         SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE);
        return calcfgResultFromServerResult(serverResult);
      }
      supletDefinitionCalcfgSession =
          beginDefinitionCalcfgSession();
      if (supletDefinitionCalcfgSession == nullptr) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_RAM_LIMIT_EXCEEDED,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE,
                         0,
                         begin.DefinitionId,
                         begin.DefinitionVersion);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      supletDefinitionCalcfgSession->active = true;
      supletDefinitionCalcfgSession->sessionId = begin.SessionId;
      supletDefinitionCalcfgSession->lastActivityMs = nowMs;
      supletDefinitionCalcfgSession->definitionId = begin.DefinitionId;
      supletDefinitionCalcfgSession->definitionVersion =
          begin.DefinitionVersion;
      supletDefinitionCalcfgSession->jsonSize = begin.JsonSize;
      supletDefinitionCalcfgSession->cacheHandle = cacheHandle;
      memcpy(supletDefinitionCalcfgSession->expectedSha256,
             begin.JsonSha256,
             sizeof(supletDefinitionCalcfgSession->expectedSha256));
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_VALIDATE,
                       0,
                       begin.DefinitionId,
                       begin.DefinitionVersion);
      return SUPLA_CALCFG_RESULT_DONE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_CHUNK: {
      const size_t headerSize = offsetof(TCalCfg_SupletDefinitionChunk, Data);
      if (request->DataSize < headerSize ||
          request->DataSize > sizeof(TCalCfg_SupletDefinitionChunk)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_TRANSFER_TEMPLATE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletDefinitionChunk chunk = {};
      memcpy(&chunk, request->Data, request->DataSize);
      if (supletDefinitionCalcfgSession == nullptr ||
          !supletDefinitionCalcfgSession->active ||
          chunk.SessionId != supletDefinitionCalcfgSession->sessionId ||
          chunk.Size > SUPLA_CALCFG_SUPLET_DEFINITION_CHUNK_MAXSIZE ||
          request->DataSize != headerSize + chunk.Size ||
          chunk.Offset != supletDefinitionCalcfgSession->receivedSize ||
          static_cast<uint32_t>(chunk.Offset) + chunk.Size >
              supletDefinitionCalcfgSession->jsonSize) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_TRANSFER_TEMPLATE,
                         0,
                         supletDefinitionCalcfgSession
                             ? supletDefinitionCalcfgSession->definitionId
                             : 0,
                         supletDefinitionCalcfgSession
                             ? supletDefinitionCalcfgSession->definitionVersion
                             : 0);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      supletDefinitionCalcfgSession->sha256.update(
          reinterpret_cast<const uint8_t *>(chunk.Data), chunk.Size);
      supletDefinitionCalcfgSession->lastActivityMs = nowMs;
      uint16_t copied = 0;
      while (copied < chunk.Size) {
        const uint16_t freeSpace =
            SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE -
            supletDefinitionCalcfgSession->currentChunkSize;
        const uint16_t toCopy =
            chunk.Size - copied > freeSpace ? freeSpace : chunk.Size - copied;
        memcpy(supletDefinitionCalcfgSession->currentChunk +
                   supletDefinitionCalcfgSession->currentChunkSize,
               chunk.Data + copied,
               toCopy);
        supletDefinitionCalcfgSession->currentChunkSize += toCopy;
        supletDefinitionCalcfgSession->receivedSize += toCopy;
        copied += toCopy;
        if (supletDefinitionCalcfgSession->currentChunkSize ==
            SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE) {
          auto flushResult = flushDefinitionCalcfgChunk(
              supletServerConfigHandler, supletDefinitionCalcfgSession);
          if (flushResult != Supla::Suplet::ServerConfigResult::Applied) {
            fillSupletResult(result,
                             supletDetailFromServerResult(flushResult),
                             SUPLA_CALCFG_SUPLET_PHASE_TRANSFER_TEMPLATE,
                             0,
                             supletDefinitionCalcfgSession->definitionId,
                             supletDefinitionCalcfgSession->definitionVersion,
                             supletDefinitionCalcfgSession->jsonSize,
                             supletDefinitionCalcfgSession->receivedSize);
            return calcfgResultFromServerResult(flushResult);
          }
        }
      }
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_TRANSFER_TEMPLATE,
                       0,
                       supletDefinitionCalcfgSession->definitionId,
                       supletDefinitionCalcfgSession->definitionVersion,
                       supletDefinitionCalcfgSession->jsonSize,
                       supletDefinitionCalcfgSession->receivedSize);
      return SUPLA_CALCFG_RESULT_DONE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_COMMIT: {
      if (request->DataSize != sizeof(TCalCfg_SupletSessionRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletSessionRequest sessionRequest = {};
      memcpy(&sessionRequest, request->Data, sizeof(sessionRequest));
      if (supletDefinitionCalcfgSession == nullptr ||
          !supletDefinitionCalcfgSession->active ||
          sessionRequest.SessionId !=
              supletDefinitionCalcfgSession->sessionId ||
          supletDefinitionCalcfgSession->receivedSize !=
              supletDefinitionCalcfgSession->jsonSize) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE,
                         0,
                         supletDefinitionCalcfgSession
                             ? supletDefinitionCalcfgSession->definitionId
                             : 0,
                         supletDefinitionCalcfgSession
                             ? supletDefinitionCalcfgSession->definitionVersion
                             : 0,
                         supletDefinitionCalcfgSession
                             ? supletDefinitionCalcfgSession->jsonSize
                             : 0,
                         supletDefinitionCalcfgSession
                             ? supletDefinitionCalcfgSession->receivedSize
                             : 0);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      auto flushResult = flushDefinitionCalcfgChunk(
          supletServerConfigHandler, supletDefinitionCalcfgSession);
      if (flushResult != Supla::Suplet::ServerConfigResult::Applied) {
        fillSupletResult(result,
                         supletDetailFromServerResult(flushResult),
                         SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE,
                         0,
                         supletDefinitionCalcfgSession->definitionId,
                         supletDefinitionCalcfgSession->definitionVersion);
        clearDefinitionCalcfgSession();
        supletDefinitionCalcfgSession = nullptr;
        return calcfgResultFromServerResult(flushResult);
      }
      uint8_t calculatedSha[32] = {};
      supletDefinitionCalcfgSession->sha256.digest(
          calculatedSha, sizeof(calculatedSha));
      if (memcmp(calculatedSha,
                 supletDefinitionCalcfgSession->expectedSha256,
                 sizeof(calculatedSha)) != 0) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_SHA_MISMATCH,
                         SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE,
                         0,
                         supletDefinitionCalcfgSession->definitionId,
                         supletDefinitionCalcfgSession->definitionVersion);
        supletServerConfigHandler->abortStagedDownloadedDefinition(
            supletDefinitionCalcfgSession->cacheHandle);
        clearDefinitionCalcfgSession();
        supletDefinitionCalcfgSession = nullptr;
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      auto serverResult =
          supletServerConfigHandler->commitStagedDownloadedDefinition(
          supletDefinitionCalcfgSession->cacheHandle,
          supletDefinitionCalcfgSession->definitionId,
          supletDefinitionCalcfgSession->definitionVersion,
          supletDefinitionCalcfgSession->jsonSize,
          supletDefinitionCalcfgSession->expectedSha256);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE,
                       0,
                       supletDefinitionCalcfgSession->definitionId,
                       supletDefinitionCalcfgSession->definitionVersion);
      if (serverResult == Supla::Suplet::ServerConfigResult::Applied) {
        supletDefinitionCalcfgSession->active = false;
      }
      clearDefinitionCalcfgSession();
      supletDefinitionCalcfgSession = nullptr;
      return calcfgResultFromServerResult(serverResult);
    }

    case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_ABORT: {
      if (request->DataSize != sizeof(TCalCfg_SupletSessionRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletSessionRequest sessionRequest = {};
      memcpy(&sessionRequest, request->Data, sizeof(sessionRequest));
      if (supletDefinitionCalcfgSession != nullptr &&
          supletDefinitionCalcfgSession->active &&
          sessionRequest.SessionId ==
              supletDefinitionCalcfgSession->sessionId) {
        supletServerConfigHandler->abortStagedDownloadedDefinition(
            supletDefinitionCalcfgSession->cacheHandle);
        clearDefinitionCalcfgSession();
        supletDefinitionCalcfgSession = nullptr;
      }
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_NONE);
      return SUPLA_CALCFG_RESULT_DONE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_REMOVE: {
      if (request->DataSize != sizeof(TCalCfg_SupletDefinitionRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletDefinitionRequest definitionRequest = {};
      memcpy(&definitionRequest, request->Data, sizeof(definitionRequest));
      auto serverResult = supletServerConfigHandler->removeDownloadedDefinition(
          definitionRequest.DefinitionId, definitionRequest.DefinitionVersion);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_NONE,
                       0,
                       definitionRequest.DefinitionId,
                       definitionRequest.DefinitionVersion);
      return calcfgResultFromServerResult(serverResult);
    }

    case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN: {
      if (request->DataSize != sizeof(TCalCfg_SupletInstanceBegin)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      if (supletCalcfgSession != nullptr && supletCalcfgSession->active) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_BUSY,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceBegin begin = {};
      memcpy(&begin, request->Data, sizeof(begin));
      if (begin.SessionId == 0 || begin.DefinitionId == 0 ||
          begin.DefinitionVersion == 0 ||
          begin.ParamsSize > SUPLA_SUPLET_MAX_CONFIG_SIZE) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE,
                         begin.InstanceId,
                         begin.DefinitionId,
                         begin.DefinitionVersion,
                         begin.ParamsSize,
                         SUPLA_SUPLET_MAX_CONFIG_SIZE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      supletCalcfgSession = beginInstanceCalcfgSession();
      if (supletCalcfgSession == nullptr) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_RAM_LIMIT_EXCEEDED,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE,
                         begin.InstanceId,
                         begin.DefinitionId,
                         begin.DefinitionVersion);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      supletCalcfgSession->active = true;
      supletCalcfgSession->sessionId = begin.SessionId;
      supletCalcfgSession->lastActivityMs = nowMs;
      supletCalcfgSession->instanceId = begin.InstanceId;
      supletCalcfgSession->definitionId = begin.DefinitionId;
      supletCalcfgSession->definitionVersion = begin.DefinitionVersion;
      supletCalcfgSession->paramsSize = begin.ParamsSize;
      memcpy(supletCalcfgSession->expectedSha256,
             begin.ParamsSha256,
             sizeof(supletCalcfgSession->expectedSha256));
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_VALIDATE,
                       begin.InstanceId,
                       begin.DefinitionId,
                       begin.DefinitionVersion);
      return SUPLA_CALCFG_RESULT_DONE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_CHUNK: {
      const size_t headerSize = offsetof(TCalCfg_SupletInstanceChunk, Data);
      if (request->DataSize < headerSize ||
          request->DataSize > sizeof(TCalCfg_SupletInstanceChunk)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceChunk chunk = {};
      memcpy(&chunk, request->Data, request->DataSize);
      if (supletCalcfgSession == nullptr || !supletCalcfgSession->active ||
          chunk.SessionId != supletCalcfgSession->sessionId ||
          chunk.Size > SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE ||
          request->DataSize != headerSize + chunk.Size ||
          static_cast<uint32_t>(chunk.Offset) + chunk.Size >
              supletCalcfgSession->paramsSize) {
        fillSupletResult(
            result,
            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
            SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG,
            supletCalcfgSession ? supletCalcfgSession->instanceId : 0,
            supletCalcfgSession ? supletCalcfgSession->definitionId : 0,
            supletCalcfgSession ? supletCalcfgSession->definitionVersion : 0);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      memcpy(
          supletCalcfgSession->params + chunk.Offset, chunk.Data, chunk.Size);
      supletCalcfgSession->lastActivityMs = nowMs;
      for (uint8_t i = 0; i < chunk.Size; i++) {
        uint16_t index = chunk.Offset + i;
        if (!supletCalcfgSession->received[index]) {
          supletCalcfgSession->received[index] = 1;
          supletCalcfgSession->receivedSize++;
        }
      }
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG,
                       supletCalcfgSession->instanceId,
                       supletCalcfgSession->definitionId,
                       supletCalcfgSession->definitionVersion,
                       supletCalcfgSession->paramsSize,
                       supletCalcfgSession->receivedSize);
      return SUPLA_CALCFG_RESULT_DONE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_COMMIT: {
      if (request->DataSize != sizeof(TCalCfg_SupletSessionRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletSessionRequest sessionRequest = {};
      memcpy(&sessionRequest, request->Data, sizeof(sessionRequest));
      if (supletCalcfgSession == nullptr || !supletCalcfgSession->active ||
          sessionRequest.SessionId != supletCalcfgSession->sessionId ||
          supletCalcfgSession->receivedSize !=
              supletCalcfgSession->paramsSize) {
        fillSupletResult(
            result,
            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
            SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE,
            supletCalcfgSession ? supletCalcfgSession->instanceId : 0,
            supletCalcfgSession ? supletCalcfgSession->definitionId : 0,
            supletCalcfgSession ? supletCalcfgSession->definitionVersion : 0,
            supletCalcfgSession ? supletCalcfgSession->paramsSize : 0,
            supletCalcfgSession ? supletCalcfgSession->receivedSize : 0);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      uint8_t calculatedSha[32] = {};
      calculateSha256(supletCalcfgSession->params,
                      supletCalcfgSession->paramsSize,
                      calculatedSha);
      if (memcmp(calculatedSha,
                 supletCalcfgSession->expectedSha256,
                 sizeof(calculatedSha)) != 0) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_SHA_MISMATCH,
                         SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG,
                         supletCalcfgSession->instanceId,
                         supletCalcfgSession->definitionId,
                         supletCalcfgSession->definitionVersion);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      supletCalcfgSession->params[supletCalcfgSession->paramsSize] = '\0';
      uint8_t appliedInstanceId = supletCalcfgSession->instanceId;
      auto serverResult = supletServerConfigHandler->applyInstanceParams(
          supletCalcfgSession->instanceId,
          supletCalcfgSession->definitionId,
          supletCalcfgSession->definitionVersion,
          reinterpret_cast<const char *>(supletCalcfgSession->params),
          supletCalcfgSession->paramsSize,
          &appliedInstanceId);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE,
                       appliedInstanceId,
                       supletCalcfgSession->definitionId,
                       supletCalcfgSession->definitionVersion);
      clearInstanceCalcfgSession();
      supletCalcfgSession = nullptr;
      return calcfgResultFromServerResult(serverResult);
    }

    case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_REMOVE: {
      if (request->DataSize != sizeof(TCalCfg_SupletInstanceRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceRequest instanceRequest = {};
      memcpy(&instanceRequest, request->Data, sizeof(instanceRequest));
      auto serverResult = supletServerConfigHandler->removeAssignment(
          instanceRequest.InstanceId);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE,
                       instanceRequest.InstanceId);
      return calcfgResultFromServerResult(serverResult);
    }

    case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_ABORT: {
      if (request->DataSize != sizeof(TCalCfg_SupletSessionRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletSessionRequest sessionRequest = {};
      memcpy(&sessionRequest, request->Data, sizeof(sessionRequest));
      if (supletCalcfgSession != nullptr && supletCalcfgSession->active &&
          sessionRequest.SessionId == supletCalcfgSession->sessionId) {
        clearInstanceCalcfgSession();
        supletCalcfgSession = nullptr;
      }
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_NONE);
      return SUPLA_CALCFG_RESULT_DONE;
    }
  }

  fillSupletResult(result,
                   SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                   SUPLA_CALCFG_SUPLET_PHASE_NONE);
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
