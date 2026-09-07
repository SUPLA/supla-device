// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <stddef.h>
#include <string.h>
#include <supla-common/proto_suplet.h>
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
    case Supla::Suplet::ServerConfigResult::InstanceNotFound:
      return SUPLA_CALCFG_SUPLET_RESULT_INSTANCE_NOT_FOUND;
    case Supla::Suplet::ServerConfigResult::VersionMismatch:
      return SUPLA_CALCFG_SUPLET_RESULT_VERSION_MISMATCH;
    case Supla::Suplet::ServerConfigResult::Busy:
      return SUPLA_CALCFG_SUPLET_RESULT_BUSY;
    case Supla::Suplet::ServerConfigResult::StorageError:
    default:
      return SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR;
  }
}

int calcfgResultFromServerResult(Supla::Suplet::ServerConfigResult result) {
  switch (result) {
    case Supla::Suplet::ServerConfigResult::Applied:
    case Supla::Suplet::ServerConfigResult::Removed:
      return SUPLA_CALCFG_RESULT_DONE;
    case Supla::Suplet::ServerConfigResult::DefinitionNotFound:
    case Supla::Suplet::ServerConfigResult::InstanceNotFound:
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

uint16_t getBuiltinDefinitionJsonSize(
    const Supla::Suplet::Definition *definition) {
  if (definition == nullptr || definition->definitionJson == nullptr) {
    return 0;
  }
  if (definition->definitionJsonSize != 0) {
    return definition->definitionJsonSize;
  }
  const size_t size = strlen(definition->definitionJson);
  return size > UINT16_MAX ? 0 : static_cast<uint16_t>(size);
}

uint16_t getStorageChunkCapacity(
    const Supla::Suplet::CalcfgSession *session) {
  if (session == nullptr) {
    return 0;
  }
  return session->type == Supla::Suplet::CalcfgTransferType::Definition
             ? SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE
             : SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE;
}

Supla::Suplet::ServerConfigResult flushStorageChunk(
    Supla::Suplet::Manager *manager,
    Supla::Suplet::ServerConfigHandler *handler,
    Supla::Suplet::CalcfgSession *session) {
  if (manager == nullptr || handler == nullptr || session == nullptr) {
    return Supla::Suplet::ServerConfigResult::InvalidArgument;
  }
  if (session->storageChunkSize == 0) {
    return Supla::Suplet::ServerConfigResult::Applied;
  }

  Supla::Suplet::ServerConfigResult result =
      Supla::Suplet::ServerConfigResult::InvalidArgument;
  if (session->type == Supla::Suplet::CalcfgTransferType::Definition) {
    result = handler->writeStagedDownloadedDefinitionChunk(
        session->definitionCacheHandle,
        session->storageChunkIndex,
        session->storageChunk,
        session->storageChunkSize);
  } else if (session->type == Supla::Suplet::CalcfgTransferType::Instance) {
    result = manager->writeStagedArtifactChunk(
                 &session->artifactStorageHandle,
                 session->storageChunk,
                 session->storageChunkSize)
                 ? Supla::Suplet::ServerConfigResult::Applied
                 : Supla::Suplet::ServerConfigResult::StorageError;
  }
  if (result == Supla::Suplet::ServerConfigResult::Applied) {
    session->storageChunkIndex++;
    session->storageChunkSize = 0;
  }
  return result;
}

bool appendStorageData(Supla::Suplet::Manager *manager,
                       Supla::Suplet::ServerConfigHandler *handler,
                       Supla::Suplet::CalcfgSession *session,
                       const uint8_t *data,
                       uint16_t size) {
  if (data == nullptr || size == 0) {
    return false;
  }
  uint16_t copied = 0;
  const uint16_t capacity = getStorageChunkCapacity(session);
  if (capacity == 0 || capacity > sizeof(session->storageChunk)) {
    return false;
  }
  while (copied < size) {
    const uint16_t available = capacity - session->storageChunkSize;
    uint16_t toCopy = size - copied;
    if (toCopy > available) {
      toCopy = available;
    }
    memcpy(session->storageChunk + session->storageChunkSize,
           data + copied,
           toCopy);
    session->storageChunkSize += toCopy;
    copied += toCopy;
    if (session->storageChunkSize == capacity &&
        flushStorageChunk(manager, handler, session) !=
            Supla::Suplet::ServerConfigResult::Applied) {
      return false;
    }
  }
  return true;
}

int failTransfer(Supla::Suplet::Manager *manager,
                 TDS_DeviceCalCfgResult *result,
                 uint8_t detail,
                 uint8_t phase) {
  uint8_t instanceId = 0;
  uint32_t definitionId = 0;
  uint16_t definitionVersion = 0;
  if (manager != nullptr && manager->getCalcfgSession() != nullptr) {
    instanceId = manager->getCalcfgSession()->instanceId;
    definitionId = manager->getCalcfgSession()->definitionId;
    definitionVersion = manager->getCalcfgSession()->definitionVersion;
  }
  fillSupletResult(result,
                   detail,
                   phase,
                   instanceId,
                   definitionId,
                   definitionVersion);
  if (manager != nullptr) {
    manager->clearCalcfgSession();
  }
  return SUPLA_CALCFG_RESULT_FALSE;
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
  auto handler = getServerConfigHandler();
  auto table = getInstanceTable();
  if (table == nullptr) {
    fillSupletResult(result,
                     SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR,
                     SUPLA_CALCFG_SUPLET_PHASE_NONE);
    return SUPLA_CALCFG_RESULT_FALSE;
  }

  switch (request->Command) {
    case SUPLA_CALCFG_CMD_SUPLET_GET_CAPABILITIES: {
      auto capabilities = getCapabilityRegistry();
      if (capabilities == nullptr) {
        return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
      }
      TCalCfg_SupletListRequest input = {};
      input.Limit = SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS;
      if (request->DataSize != 0) {
        if (request->DataSize != sizeof(input)) {
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE);
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        memcpy(&input, request->Data, sizeof(input));
      }
      TCalCfg_SupletCapabilityList output = {};
      output.Offset = input.Offset;
      output.Total = capabilities->getCount();
      uint8_t limit = input.Limit;
      if (limit == 0 || limit > SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS) {
        limit = SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS;
      }
      for (uint8_t i = 0;
           i < limit && input.Offset + i < output.Total;
           i++) {
        Capability capability = {};
        if (!capabilities->getCapability(input.Offset + i, &capability)) {
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
        item.MaxArtifactSize = capability.maxArtifactSize;
      }
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = sizeof(output);
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_LIST: {
      TCalCfg_SupletListRequest input = {};
      input.Limit = SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS;
      if (request->DataSize != 0) {
        if (request->DataSize != sizeof(input)) {
          fillSupletResult(result,
                           SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                           SUPLA_CALCFG_SUPLET_PHASE_NONE);
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        memcpy(&input, request->Data, sizeof(input));
      }
      TCalCfg_SupletDefinitionList output = {};
      output.Offset = input.Offset;
      const uint8_t builtinCount = supletRegistry->getCount();
      const uint8_t cachedCount = handler->getCachedDefinitionCount();
      const uint16_t combinedTotal = builtinCount + cachedCount;
      output.Total = combinedTotal > UINT8_MAX
                         ? UINT8_MAX
                         : static_cast<uint8_t>(combinedTotal);
      uint8_t limit = input.Limit;
      if (limit == 0 || limit > SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS) {
        limit = SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS;
      }
      for (uint8_t i = 0;
           i < limit && input.Offset + i < output.Total;
           i++) {
        const uint8_t index = input.Offset + i;
        auto &item = output.Items[output.Count++];
        if (index < builtinCount) {
          Capability capability = {};
          if (!supletRegistry->getCapability(index, &capability)) {
            output.Count--;
            break;
          }
          const auto *definition = supletRegistry->findDefinition(
              capability.definitionId, capability.minDefinitionVersion);
          item.DefinitionId = capability.definitionId;
          item.DefinitionVersion = capability.minDefinitionVersion;
          item.Size = getBuiltinDefinitionJsonSize(definition);
          item.Source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_BUILTIN;
        } else {
          CachedDefinitionDetails details = {};
          if (!handler->getCachedDefinitionDetails(
                  index - builtinCount, &details)) {
            output.Count--;
            break;
          }
          item.DefinitionId = details.cache.definitionId;
          item.DefinitionVersion = details.cache.definitionVersion;
          item.Size = details.cache.jsonSize;
          item.Source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_CACHED;
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
      TCalCfg_SupletDefinitionConfigRequest input = {};
      memcpy(&input, request->Data, sizeof(input));
      if (input.DefinitionId == 0 || input.DefinitionVersion == 0) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }

      const char *builtinJson = nullptr;
      char *cachedJson = nullptr;
      uint16_t totalSize = 0;
      uint8_t source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_BUILTIN;
      const auto *definition = supletRegistry->findDefinition(
          input.DefinitionId, input.DefinitionVersion);
      if (definition != nullptr) {
        builtinJson = definition->definitionJson;
        totalSize = getBuiltinDefinitionJsonSize(definition);
      } else {
        source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_CACHED;
        CachedDefinitionInfo info = {};
        cachedJson = new char[SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1];
        if (cachedJson == nullptr ||
            !handler->loadDownloadedDefinitionJson(
                input.DefinitionId,
                input.DefinitionVersion,
                cachedJson,
                SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1,
                &info)) {
          delete[] cachedJson;
          return SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
        }
        totalSize = info.jsonSize;
      }
      if (totalSize == 0 || input.Offset >= totalSize) {
        delete[] cachedJson;
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      uint8_t maxSize = input.MaxSize;
      if (maxSize == 0 || maxSize > SUPLA_CALCFG_SUPLET_DATA_CHUNK_MAXSIZE) {
        maxSize = SUPLA_CALCFG_SUPLET_DATA_CHUNK_MAXSIZE;
      }
      const uint16_t remaining = totalSize - input.Offset;
      const uint8_t size = remaining > maxSize
                               ? maxSize
                               : static_cast<uint8_t>(remaining);
      TCalCfg_SupletDefinitionConfigChunk output = {};
      output.DefinitionId = input.DefinitionId;
      output.DefinitionVersion = input.DefinitionVersion;
      output.Offset = input.Offset;
      output.TotalSize = totalSize;
      output.Source = source;
      output.Size = size;
      memcpy(output.Data,
             source == SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_BUILTIN
                 ? builtinJson + input.Offset
                 : cachedJson + input.Offset,
             size);
      delete[] cachedJson;
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize =
          offsetof(TCalCfg_SupletDefinitionConfigChunk, Data) + size;
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_LIST: {
      if (request->DataSize != sizeof(TCalCfg_SupletListRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletListRequest input = {};
      memcpy(&input, request->Data, sizeof(input));
      TCalCfg_SupletInstanceList output = {};
      output.Offset = input.Offset;
      output.Total = table->getCount();
      uint8_t limit = input.Limit;
      if (limit == 0 || limit > SUPLA_CALCFG_SUPLET_INSTANCE_LIST_MAX_ITEMS) {
        limit = SUPLA_CALCFG_SUPLET_INSTANCE_LIST_MAX_ITEMS;
      }
      for (uint8_t i = 0;
           i < limit && input.Offset + i < table->getCount();
           i++) {
        const InstanceRecord *record = table->getRecord(input.Offset + i);
        if (record == nullptr) {
          break;
        }
        auto &item = output.Items[output.Count++];
        item.InstanceId = record->instanceId;
        item.SubDeviceId = record->subDeviceId;
        item.ChannelCount = record->channelMap.getCount();
        item.DefinitionId = record->definitionId;
        item.DefinitionVersion = record->definitionVersion;
        item.ConfigSize = record->configSize;
        item.Revision = record->revision;
        item.ArtifactSize = record->artifactSize;
      }
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = sizeof(output);
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_DATA: {
      if (request->DataSize != sizeof(TCalCfg_SupletInstanceDataRequest)) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                         SUPLA_CALCFG_SUPLET_PHASE_NONE);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceDataRequest input = {};
      memcpy(&input, request->Data, sizeof(input));
      const InstanceRecord *indexed = table->findByInstanceId(input.InstanceId);
      if (indexed == nullptr ||
          (input.Part != SUPLA_CALCFG_SUPLET_TRANSFER_PART_CONFIG &&
           input.Part != SUPLA_CALCFG_SUPLET_TRANSFER_PART_ARTIFACT)) {
        return SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
      }
      InstanceRecord record = {};
      if (!loadInstance(input.InstanceId, &record)) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      const uint32_t totalSize =
          input.Part == SUPLA_CALCFG_SUPLET_TRANSFER_PART_CONFIG
              ? record.configSize
              : record.artifactSize;
      if (input.Offset > totalSize ||
          (totalSize > 0 && input.Offset == totalSize)) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      uint8_t maxSize = input.MaxSize;
      if (maxSize == 0 || maxSize > SUPLA_CALCFG_SUPLET_DATA_CHUNK_MAXSIZE) {
        maxSize = SUPLA_CALCFG_SUPLET_DATA_CHUNK_MAXSIZE;
      }
      const uint32_t remaining = totalSize - input.Offset;
      const uint8_t size = remaining > maxSize
                               ? maxSize
                               : static_cast<uint8_t>(remaining);
      TCalCfg_SupletInstanceDataChunk output = {};
      output.InstanceId = input.InstanceId;
      output.Part = input.Part;
      output.Offset = input.Offset;
      output.TotalSize = totalSize;
      output.Size = size;
      bool loaded = true;
      if (size > 0 && input.Part == SUPLA_CALCFG_SUPLET_TRANSFER_PART_CONFIG) {
        loaded = record.config != nullptr &&
                 input.Offset + size <= record.configSize;
        if (loaded) {
          memcpy(output.Data, record.config + input.Offset, size);
        }
      } else if (size > 0) {
        loaded = readArtifact(input.InstanceId,
                              input.Offset,
                              reinterpret_cast<uint8_t *>(output.Data),
                              size);
      }
      if (!loaded) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = offsetof(TCalCfg_SupletInstanceDataChunk, Data) + size;
      return SUPLA_CALCFG_RESULT_TRUE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN: {
      CalcfgSession *session = beginCalcfgSession();
      if (session == nullptr ||
          request->DataSize != sizeof(TCalCfg_SupletDefinitionBegin)) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
      }
      TCalCfg_SupletDefinitionBegin input = {};
      memcpy(&input, request->Data, sizeof(input));
      session->type = CalcfgTransferType::Definition;
      session->definitionId = input.DefinitionId;
      session->definitionVersion = input.DefinitionVersion;
      session->definitionSize = input.Size;
      if (!isServerDefinitionId(input.DefinitionId) ||
          input.DefinitionVersion == 0 || input.Size == 0 ||
          input.Size > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
      }
      const auto serverResult = handler->beginStagedDownloadedDefinition(
          input.DefinitionId,
          input.DefinitionVersion,
          input.Size,
          &session->definitionCacheHandle);
      if (serverResult != ServerConfigResult::Applied) {
        return failTransfer(this,
                            result,
                            supletDetailFromServerResult(serverResult),
                            SUPLA_CALCFG_SUPLET_PHASE_TRANSFER_TEMPLATE);
      }
      session->active = true;
      session->lastActivityMs = nowMs;
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_TRANSFER_TEMPLATE,
                       0,
                       input.DefinitionId,
                       input.DefinitionVersion,
                       input.Size,
                       0);
      return SUPLA_CALCFG_RESULT_DONE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN: {
      CalcfgSession *session = beginCalcfgSession();
      if (session == nullptr ||
          request->DataSize != sizeof(TCalCfg_SupletInstanceBegin)) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
      }
      TCalCfg_SupletInstanceBegin input = {};
      memcpy(&input, request->Data, sizeof(input));
      if (input.InstanceId == 0) {
        input.InstanceId = getFirstFreeSubDeviceId();
      }
      session->type = CalcfgTransferType::Instance;
      session->instanceId = input.InstanceId;
      session->definitionId = input.DefinitionId;
      session->definitionVersion = input.DefinitionVersion;
      session->revision = input.Revision;
      session->configSize = input.ConfigSize;
      session->artifactSize = input.ArtifactSize;
      const bool keepArtifact =
          (input.Flags & SUPLA_CALCFG_SUPLET_INSTANCE_FLAG_KEEP_ARTIFACT) != 0;
      const bool invalidFlags =
          (input.Flags & ~SUPLA_CALCFG_SUPLET_INSTANCE_FLAG_KEEP_ARTIFACT) != 0;
      if (input.InstanceId == 0 || input.DefinitionId == 0 ||
          input.DefinitionVersion == 0 || input.Revision == 0 ||
          input.ConfigSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
          input.ArtifactSize > SUPLA_SUPLET_MAX_ARTIFACT_SIZE ||
          invalidFlags || (keepArtifact && input.ArtifactSize != 0) ||
          (keepArtifact &&
           table->findByInstanceId(input.InstanceId) == nullptr)) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
      }
      if (keepArtifact) {
        session->artifactUpdateMode = ArtifactUpdateMode::Keep;
      } else if (input.ArtifactSize > 0) {
        session->artifactUpdateMode = ArtifactUpdateMode::Replace;
        if (!beginStagedArtifact(input.InstanceId,
                                 input.ArtifactSize,
                                 &session->artifactStorageHandle)) {
          return failTransfer(this,
                              result,
                              SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR,
                              SUPLA_CALCFG_SUPLET_PHASE_VALIDATE);
        }
      } else {
        session->artifactUpdateMode = ArtifactUpdateMode::Remove;
      }
      session->active = true;
      session->lastActivityMs = nowMs;
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_VALIDATE,
                       input.InstanceId,
                       input.DefinitionId,
                       input.DefinitionVersion);
      return SUPLA_CALCFG_RESULT_DONE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_TRANSFER_CHUNK: {
      const size_t headerSize = offsetof(TCalCfg_SupletTransferChunk, Data);
      if (request->DataSize < headerSize ||
          request->DataSize > sizeof(TCalCfg_SupletTransferChunk)) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG);
      }
      TCalCfg_SupletTransferChunk input = {};
      memcpy(&input, request->Data, request->DataSize);
      CalcfgSession *session = getCalcfgSession();
      if (session == nullptr || !session->active || input.Size == 0 ||
          input.Size > SUPLA_CALCFG_SUPLET_TRANSFER_CHUNK_MAXSIZE ||
          request->DataSize != headerSize + input.Size) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG);
      }

      bool accepted = false;
      uint32_t totalSize = 0;
      uint32_t receivedSize = 0;
      if (session->type == CalcfgTransferType::Definition &&
          input.Part == SUPLA_CALCFG_SUPLET_TRANSFER_PART_DEFINITION &&
          input.Offset == session->definitionReceivedSize &&
          input.Offset + input.Size <= session->definitionSize) {
        accepted = appendStorageData(this,
                                     handler,
                                     session,
                                     reinterpret_cast<uint8_t *>(input.Data),
                                     input.Size);
        if (accepted) {
          session->definitionReceivedSize += input.Size;
          totalSize = session->definitionSize;
          receivedSize = session->definitionReceivedSize;
        }
      } else if (session->type == CalcfgTransferType::Instance &&
                 input.Part == SUPLA_CALCFG_SUPLET_TRANSFER_PART_CONFIG &&
                 input.Offset == session->configReceivedSize &&
                 input.Offset + input.Size <= session->configSize) {
        memcpy(session->config + input.Offset, input.Data, input.Size);
        session->configReceivedSize += input.Size;
        totalSize = session->configSize;
        receivedSize = session->configReceivedSize;
        accepted = true;
      } else if (session->type == CalcfgTransferType::Instance &&
                 input.Part == SUPLA_CALCFG_SUPLET_TRANSFER_PART_ARTIFACT &&
                 session->artifactUpdateMode == ArtifactUpdateMode::Replace &&
                 input.Offset == session->artifactReceivedSize &&
                 input.Offset + input.Size <= session->artifactSize) {
        accepted = appendStorageData(this,
                                     handler,
                                     session,
                                     reinterpret_cast<uint8_t *>(input.Data),
                                     input.Size);
        if (accepted) {
          session->artifactReceivedSize += input.Size;
          totalSize = session->artifactSize;
          receivedSize = session->artifactReceivedSize;
        }
      }
      if (!accepted) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG);
      }
      session->lastActivityMs = nowMs;
      fillSupletResult(result,
                       SUPLA_CALCFG_SUPLET_RESULT_OK,
                       SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG,
                       session->instanceId,
                       session->definitionId,
                       session->definitionVersion,
                       totalSize > UINT16_MAX ? UINT16_MAX : totalSize,
                       receivedSize > UINT16_MAX ? UINT16_MAX : receivedSize);
      return SUPLA_CALCFG_RESULT_DONE;
    }

    case SUPLA_CALCFG_CMD_SUPLET_TRANSFER_COMMIT: {
      CalcfgSession *session = getCalcfgSession();
      if (request->DataSize != 0 || session == nullptr || !session->active) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE);
      }
      if (session->type == CalcfgTransferType::Definition) {
        if (session->definitionReceivedSize != session->definitionSize ||
            flushStorageChunk(this, handler, session) !=
                ServerConfigResult::Applied) {
          return failTransfer(this,
                              result,
                              SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                              SUPLA_CALCFG_SUPLET_PHASE_TRANSFER_TEMPLATE);
        }
        const auto serverResult = handler->commitStagedDownloadedDefinition(
            session->definitionCacheHandle,
            session->definitionId,
            session->definitionVersion,
            session->definitionSize);
        fillSupletResult(result,
                         supletDetailFromServerResult(serverResult),
                         SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE,
                         0,
                         session->definitionId,
                         session->definitionVersion);
        if (serverResult == ServerConfigResult::Applied) {
          session->active = false;
        }
        clearCalcfgSession();
        return calcfgResultFromServerResult(serverResult);
      }

      if (session->type != CalcfgTransferType::Instance ||
          session->configReceivedSize != session->configSize ||
          (session->artifactUpdateMode == ArtifactUpdateMode::Replace &&
           session->artifactReceivedSize != session->artifactSize) ||
          flushStorageChunk(this, handler, session) !=
              ServerConfigResult::Applied) {
        return failTransfer(this,
                            result,
                            SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST,
                            SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE);
      }
      session->config[session->configSize] = '\0';
      uint8_t appliedInstanceId = session->instanceId;
      const auto serverResult = handler->applyInstanceData(
          session->instanceId,
          session->definitionId,
          session->definitionVersion,
          session->revision,
          reinterpret_cast<const char *>(session->config),
          session->configSize,
          session->artifactSize,
          session->artifactUpdateMode == ArtifactUpdateMode::Keep,
          session->artifactUpdateMode == ArtifactUpdateMode::Replace
              ? &session->artifactStorageHandle
              : nullptr,
          &appliedInstanceId);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE,
                       appliedInstanceId,
                       session->definitionId,
                       session->definitionVersion);
      if (serverResult == ServerConfigResult::Applied) {
        session->active = false;
      }
      clearCalcfgSession();
      return calcfgResultFromServerResult(serverResult);
    }

    case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_REMOVE: {
      if (request->DataSize != sizeof(TCalCfg_SupletDefinitionRequest)) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletDefinitionRequest input = {};
      memcpy(&input, request->Data, sizeof(input));
      const auto serverResult = handler->removeDownloadedDefinition(
          input.DefinitionId, input.DefinitionVersion);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_NONE,
                       0,
                       input.DefinitionId,
                       input.DefinitionVersion);
      return calcfgResultFromServerResult(serverResult);
    }

    case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_REMOVE: {
      if (request->DataSize != sizeof(TCalCfg_SupletInstanceRequest)) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceRequest input = {};
      memcpy(&input, request->Data, sizeof(input));
      const auto serverResult = handler->removeAssignment(input.InstanceId);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE,
                       input.InstanceId);
      return calcfgResultFromServerResult(serverResult);
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
