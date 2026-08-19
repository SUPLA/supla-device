// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef supla_proto_suplet_H_
#define supla_proto_suplet_H_

#include "proto.h"

// Device registration flag.
#define SUPLA_DEVICE_FLAG_SUPLET_SUPPORTED 0x40000  // FDEV

// CALCFG commands.
#define SUPLA_CALCFG_CMD_SUPLET_GET_CAPABILITIES 9500           // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_COUNT 9510         // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_LIST 9520          // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_INFO 9530          // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_CONFIG 9540        // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN 9550           // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_DEFINITION_CHUNK 9560           // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_DEFINITION_COMMIT 9570          // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_DEFINITION_ABORT 9580           // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_DEFINITION_REMOVE 9585          // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_LIST 9586        // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_CONFIG 9587      // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN 9590             // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_CHUNK 9600             // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_COMMIT 9610            // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_REMOVE 9620            // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_ABORT 9630             // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_UPGRADE_BEGIN 9640     // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_UPGRADE_CHUNK 9650     // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_UPGRADE_COMMIT 9660    // FDEV
#define SUPLA_CALCFG_CMD_SUPLET_INSTANCE_UPGRADE_ABORT 9670     // FDEV

// Results returned by the Suplet CALCFG handler.
#define SUPLA_CALCFG_SUPLET_RESULT_OK 0
#define SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST 1
#define SUPLA_CALCFG_SUPLET_RESULT_UNSUPPORTED_DEFINITION 2
#define SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_NOT_FOUND 3
#define SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_TRANSFER_FAILED 4
#define SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_SHA_MISMATCH 5
#define SUPLA_CALCFG_SUPLET_RESULT_INSTANCE_LIMIT_EXCEEDED 6
#define SUPLA_CALCFG_SUPLET_RESULT_CHANNEL_LIMIT_EXCEEDED 7
#define SUPLA_CALCFG_SUPLET_RESULT_RAM_LIMIT_EXCEEDED 8
#define SUPLA_CALCFG_SUPLET_RESULT_CONFIG_TOO_LARGE 9
#define SUPLA_CALCFG_SUPLET_RESULT_INVALID_CONFIG 10
#define SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR 11
#define SUPLA_CALCFG_SUPLET_RESULT_BUSY 12
#define SUPLA_CALCFG_SUPLET_RESULT_CREATE_ONLY_PARAM_CHANGED 13
#define SUPLA_CALCFG_SUPLET_RESULT_TOPOLOGY_CHANGE_NOT_ALLOWED 14
#define SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_CANNOT_BE_CHANGED 15
#define SUPLA_CALCFG_SUPLET_RESULT_INVALID_DEFINITION 16
#define SUPLA_CALCFG_SUPLET_RESULT_INSTANCE_NOT_FOUND 17
#define SUPLA_CALCFG_SUPLET_RESULT_VERSION_MISMATCH 18

// Suplet processing phases.
#define SUPLA_CALCFG_SUPLET_PHASE_NONE 0
#define SUPLA_CALCFG_SUPLET_PHASE_VALIDATE 1
#define SUPLA_CALCFG_SUPLET_PHASE_TRANSFER_TEMPLATE 2
#define SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE 3
#define SUPLA_CALCFG_SUPLET_PHASE_VALIDATE_CONFIG 4
#define SUPLA_CALCFG_SUPLET_PHASE_ALLOCATE_CHANNELS 5
#define SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE 6
#define SUPLA_CALCFG_SUPLET_PHASE_RUNTIME_REFRESH 7

#define SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS 4
#define SUPLA_CALCFG_SUPLET_INSTANCE_LIST_MAX_ITEMS 5
#define SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS 2
#define SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE 100
#define SUPLA_CALCFG_SUPLET_DEFINITION_CHUNK_MAXSIZE 112
#define SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE 112

#define SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_BUILTIN 1
#define SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_CACHED 2

#pragma pack(push, 1)

typedef struct {
  unsigned char Version;
  unsigned char DetailCode;  // SUPLA_CALCFG_SUPLET_RESULT_*
  unsigned char Phase;       // SUPLA_CALCFG_SUPLET_PHASE_*
  unsigned char Flags;
  unsigned char InstanceId;
  unsigned char Reserved1;
  unsigned char Reserved2;
  unsigned char Reserved3;
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned _supla_int16_t Required;
  unsigned _supla_int16_t Available;
} TCalCfg_SupletResult;  // FDEV

typedef struct {
  unsigned char Offset;
  unsigned char Limit;
} TCalCfg_SupletListRequest;  // FDEV

typedef struct {
  unsigned char Category;
  unsigned char Kind;
  unsigned char MinSchemaVersion;
  unsigned char MaxSchemaVersion;
  unsigned char HandlerVersion;
  unsigned char MaxInstances;
  unsigned char SupportsDownloadedDefinition;
  unsigned char Reserved;
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t MinDefinitionVersion;
  unsigned _supla_int16_t MaxDefinitionVersion;
} TCalCfg_SupletCapabilityItem;  // FDEV

typedef struct {
  unsigned char Offset;
  unsigned char Count;
  unsigned char Total;
  unsigned char Reserved;
  TCalCfg_SupletCapabilityItem
      Items[SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS];
} TCalCfg_SupletCapabilityList;  // FDEV

typedef struct {
  unsigned char Category;
  unsigned char Kind;
  unsigned char SchemaVersion;
  unsigned char HandlerVersion;
  unsigned char MaxInstances;
  unsigned char Source;  // SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_*
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned _supla_int16_t JsonSize;
  unsigned char JsonSha256[32];
} TCalCfg_SupletDefinitionListItem;  // FDEV

typedef struct {
  unsigned char Offset;
  unsigned char Count;
  unsigned char Total;
  unsigned char Reserved;
  TCalCfg_SupletDefinitionListItem
      Items[SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS];
} TCalCfg_SupletDefinitionList;  // FDEV

typedef struct {
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned _supla_int16_t Offset;
  unsigned char MaxSize;
  unsigned char Reserved;
} TCalCfg_SupletDefinitionConfigRequest;  // FDEV

typedef struct {
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned _supla_int16_t Offset;
  unsigned _supla_int16_t TotalSize;
  unsigned char Source;  // SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_*
  unsigned char Size;
  char Data[SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE];
} TCalCfg_SupletDefinitionConfigChunk;  // FDEV

typedef struct {
  unsigned char Count;
  unsigned char MaxInstances;
  unsigned char MaxChannelsPerInstance;
  unsigned char MaxCachedDefinitions;
} TCalCfg_SupletInstanceCount;  // FDEV

typedef struct {
  unsigned char InstanceId;
  unsigned char Reserved1;
  unsigned char Reserved2;
  unsigned char Reserved3;
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned char SubDeviceId;
  unsigned char ChannelCount;
  unsigned char Reserved4;
  unsigned char Reserved5;
} TCalCfg_SupletInstanceListItem;  // FDEV

typedef struct {
  unsigned char Offset;
  unsigned char Count;
  unsigned char Total;
  unsigned char Reserved;
  TCalCfg_SupletInstanceListItem
      Items[SUPLA_CALCFG_SUPLET_INSTANCE_LIST_MAX_ITEMS];
} TCalCfg_SupletInstanceList;  // FDEV

typedef struct {
  unsigned char InstanceId;
  unsigned char Reserved1;
  unsigned char Reserved2;
  unsigned char Reserved3;
} TCalCfg_SupletInstanceRequest;  // FDEV

typedef struct {
  unsigned char InstanceId;
  unsigned char Reserved1;
  unsigned char Reserved2;
  unsigned char Reserved3;
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned char SubDeviceId;
  unsigned char ChannelCount;
  unsigned char Reserved4;
  unsigned char Reserved5;
  unsigned _supla_int16_t ParamsSize;
  unsigned char ParamsSha256[32];
} TCalCfg_SupletInstanceInfo;  // FDEV

typedef struct {
  unsigned char InstanceId;
  unsigned char Reserved1;
  unsigned char Reserved2;
  unsigned char Reserved3;
  unsigned _supla_int16_t Offset;
  unsigned char MaxSize;
  unsigned char Reserved;
} TCalCfg_SupletInstanceConfigRequest;  // FDEV

typedef struct {
  unsigned char InstanceId;
  unsigned char Reserved1;
  unsigned char Reserved2;
  unsigned char Reserved3;
  unsigned _supla_int16_t Offset;
  unsigned _supla_int16_t TotalSize;
  unsigned char Size;
  char Data[SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE];
} TCalCfg_SupletInstanceConfigChunk;  // FDEV

typedef struct {
  unsigned _supla_int_t SessionId;
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned _supla_int16_t JsonSize;
  unsigned char JsonSha256[32];
  unsigned char Flags;
  unsigned char Reserved;
} TCalCfg_SupletDefinitionBegin;  // FDEV

typedef struct {
  unsigned _supla_int_t SessionId;
  unsigned _supla_int16_t Offset;
  unsigned char Size;
  char Data[SUPLA_CALCFG_SUPLET_DEFINITION_CHUNK_MAXSIZE];
} TCalCfg_SupletDefinitionChunk;  // FDEV

typedef struct {
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned _supla_int16_t Reserved;
} TCalCfg_SupletDefinitionRequest;  // FDEV

typedef struct {
  unsigned _supla_int_t SessionId;
  unsigned char InstanceId;
  unsigned char Reserved1;
  unsigned char Reserved2;
  unsigned char Reserved3;
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t DefinitionVersion;
  unsigned _supla_int16_t ParamsSize;
  unsigned char ParamsSha256[32];
  unsigned char Flags;
  unsigned char Reserved4;
} TCalCfg_SupletInstanceBegin;  // FDEV

typedef struct {
  unsigned _supla_int_t SessionId;
  unsigned char InstanceId;
  unsigned char Reserved1;
  unsigned char Reserved2;
  unsigned char Reserved3;
  unsigned _supla_int_t DefinitionId;
  unsigned _supla_int16_t FromDefinitionVersion;
  unsigned _supla_int16_t ToDefinitionVersion;
  unsigned _supla_int16_t ParamsSize;
  unsigned char ParamsSha256[32];
  unsigned char Flags;
  unsigned char Reserved4;
} TCalCfg_SupletInstanceUpgradeBegin;  // FDEV

typedef struct {
  unsigned _supla_int_t SessionId;
  unsigned _supla_int16_t Offset;
  unsigned char Size;
  char Data[SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE];
} TCalCfg_SupletInstanceChunk;  // FDEV

typedef struct {
  unsigned _supla_int_t SessionId;
} TCalCfg_SupletSessionRequest;  // FDEV

#pragma pack(pop)

#ifdef __cplusplus
// Protocol size checks.
static_assert((unsigned int)18 == sizeof(TCalCfg_SupletResult));
static_assert((unsigned int)2 == sizeof(TCalCfg_SupletListRequest));
static_assert((unsigned int)16 == sizeof(TCalCfg_SupletCapabilityItem));
static_assert((unsigned int)68 == sizeof(TCalCfg_SupletCapabilityList));
static_assert((unsigned int)46 == sizeof(TCalCfg_SupletDefinitionListItem));
static_assert((unsigned int)96 == sizeof(TCalCfg_SupletDefinitionList));
static_assert((unsigned int)10 ==
              sizeof(TCalCfg_SupletDefinitionConfigRequest));
static_assert((unsigned int)112 ==
              sizeof(TCalCfg_SupletDefinitionConfigChunk));
static_assert((unsigned int)4 == sizeof(TCalCfg_SupletInstanceCount));
static_assert((unsigned int)14 == sizeof(TCalCfg_SupletInstanceListItem));
static_assert((unsigned int)74 == sizeof(TCalCfg_SupletInstanceList));
static_assert((unsigned int)4 == sizeof(TCalCfg_SupletInstanceRequest));
static_assert((unsigned int)48 == sizeof(TCalCfg_SupletInstanceInfo));
static_assert((unsigned int)8 ==
              sizeof(TCalCfg_SupletInstanceConfigRequest));
static_assert((unsigned int)109 ==
              sizeof(TCalCfg_SupletInstanceConfigChunk));
static_assert((unsigned int)46 == sizeof(TCalCfg_SupletDefinitionBegin));
static_assert((unsigned int)119 == sizeof(TCalCfg_SupletDefinitionChunk));
static_assert((unsigned int)8 == sizeof(TCalCfg_SupletDefinitionRequest));
static_assert((unsigned int)50 == sizeof(TCalCfg_SupletInstanceBegin));
static_assert((unsigned int)52 ==
              sizeof(TCalCfg_SupletInstanceUpgradeBegin));
static_assert((unsigned int)119 == sizeof(TCalCfg_SupletInstanceChunk));
static_assert((unsigned int)4 == sizeof(TCalCfg_SupletSessionRequest));

static_assert(sizeof(TCalCfg_SupletResult) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletListRequest) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletCapabilityItem) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletCapabilityList) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletDefinitionListItem) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletDefinitionList) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletDefinitionConfigRequest) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletDefinitionConfigChunk) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceCount) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceListItem) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceList) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceRequest) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceInfo) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceConfigRequest) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceConfigChunk) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletDefinitionBegin) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletDefinitionChunk) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletDefinitionRequest) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceBegin) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceUpgradeBegin) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletInstanceChunk) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
static_assert(sizeof(TCalCfg_SupletSessionRequest) <=
              (unsigned int)SUPLA_CALCFG_DATA_MAXSIZE);
#endif

#endif /* supla_proto_suplet_H_ */
