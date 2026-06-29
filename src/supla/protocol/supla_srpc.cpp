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

#include "supla_srpc.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>
#include <SuplaDevice.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <supla-common/srpc.h>
#include <supla/channels/channel.h>
#include <supla/clock/clock.h>
#include <supla/device/channel_conflict_resolver.h>
#include <supla/device/register_device.h>
#include <supla/device/remote_device_config.h>
#include <supla/device/supla_ca_cert.h>
#include <supla/log_wrapper.h>
#include <supla/network/client.h>
#include <supla/network/network.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/tools.h>

namespace Supla::Protocol {
struct CalCfgResultPendingItem {
  int16_t channelNo = 0;
  int32_t receiverId = 0;
  int32_t command = 0;
  uint32_t createdAtMs = 0;
  uint32_t timeoutMs = 0;

  CalCfgResultPendingItem *next = nullptr;
};
}  // namespace Supla::Protocol

bool Supla::Protocol::SuplaSrpc::isSuplaSSLEnabled = true;

static const char wrongCert[] = "SUPLA";

namespace {
void logRawHexDump(const char *direction,
                   int callId,
                   const char *callName,
                   const uint8_t *buf,
                   size_t size,
                   bool verbose);

const char *safeCallName(int callId) {
  switch (callId) {
    case SUPLA_DCS_CALL_GETVERSION:
      return "GETVERSION";
    case SUPLA_SDC_CALL_GETVERSION_RESULT:
      return "GETVERSION_RESULT";
    case SUPLA_SDC_CALL_VERSIONERROR:
      return "VERSIONERROR";
    case SUPLA_DCS_CALL_PING_SERVER:
      return "PING";
    case SUPLA_SDC_CALL_PING_SERVER_RESULT:
      return "PONG";
    case SUPLA_DCS_CALL_SET_ACTIVITY_TIMEOUT:
      return "SET_ACTIVITY_TIMEOUT";
    case SUPLA_SDC_CALL_SET_ACTIVITY_TIMEOUT_RESULT:
      return "SET_ACTIVITY_TIMEOUT_RESULT";
    case SUPLA_DCS_CALL_GET_REGISTRATION_ENABLED:
      return "GET_REGISTRATION_ENABLED";
    case SUPLA_SDC_CALL_GET_REGISTRATION_ENABLED_RESULT:
      return "GET_REGISTRATION_ENABLED_RESULT";
    case SUPLA_DCS_CALL_GET_USER_LOCALTIME:
      return "GET_USER_LOCALTIME";
    case SUPLA_DCS_CALL_GET_USER_LOCALTIME_RESULT:
      return "GET_USER_LOCALTIME_RESULT";
    case SUPLA_CSD_CALL_GET_CHANNEL_STATE:
      return "GET_CHANNEL_STATE";
    case SUPLA_DSC_CALL_CHANNEL_STATE_RESULT:
      return "CHANNEL_STATE_RESULT";
    case SUPLA_DS_CALL_REGISTER_DEVICE:
      return "REGISTER_DEVICE";
    case SUPLA_DS_CALL_REGISTER_DEVICE_B:
      return "REGISTER_DEVICE_B";
    case SUPLA_DS_CALL_REGISTER_DEVICE_C:
      return "REGISTER_DEVICE_C";
    case SUPLA_DS_CALL_REGISTER_DEVICE_D:
      return "REGISTER_DEVICE_D";
    case SUPLA_DS_CALL_REGISTER_DEVICE_E:
      return "REGISTER_DEVICE_E";
    case SUPLA_DS_CALL_REGISTER_DEVICE_F:
      return "REGISTER_DEVICE_F";
    case SUPLA_DS_CALL_REGISTER_DEVICE_G:
      return "REGISTER_DEVICE_G";
    case SUPLA_SD_CALL_REGISTER_DEVICE_RESULT:
      return "REGISTER_DEVICE_RESULT";
    case SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B:
      return "REGISTER_DEVICE_RESULT_B";
    case SUPLA_DS_CALL_SET_DEVICE_CONFIG:
      return "SET_DEVICE_CONFIG";
    case SUPLA_SD_CALL_SET_DEVICE_CONFIG:
      return "SET_DEVICE_CONFIG";
    case SUPLA_DS_CALL_SET_CHANNEL_CONFIG:
      return "SET_CHANNEL_CONFIG";
    case SUPLA_SD_CALL_SET_CHANNEL_CONFIG:
      return "SET_CHANNEL_CONFIG";
    case SUPLA_DS_CALL_GET_CHANNEL_CONFIG:
      return "GET_CHANNEL_CONFIG";
    case SUPLA_SD_CALL_GET_CHANNEL_CONFIG_RESULT:
      return "GET_CHANNEL_CONFIG_RESULT";
    case SUPLA_SD_CALL_SET_CHANNEL_CONFIG_RESULT:
      return "SET_CHANNEL_CONFIG_RESULT";
    case SUPLA_DS_CALL_SET_CHANNEL_CONFIG_RESULT:
      return "SET_CHANNEL_CONFIG_RESULT";
    case SUPLA_SD_CALL_SET_DEVICE_CONFIG_RESULT:
      return "SET_DEVICE_CONFIG_RESULT";
    case SUPLA_DS_CALL_SET_DEVICE_CONFIG_RESULT:
      return "SET_DEVICE_CONFIG_RESULT";
    case SUPLA_DS_CALL_GET_FIRMWARE_UPDATE_URL:
      return "GET_FIRMWARE_UPDATE_URL";
    case SUPLA_SD_CALL_GET_FIRMWARE_UPDATE_URL_RESULT:
      return "GET_FIRMWARE_UPDATE_URL_RESULT";
    case SUPLA_DCS_CALL_SET_CHANNEL_CAPTION:
      return "SET_CHANNEL_CAPTION";
    case SUPLA_SCD_CALL_SET_CHANNEL_CAPTION_RESULT:
      return "SET_CHANNEL_CAPTION_RESULT";
    case SUPLA_SD_CALL_CHANNEL_SET_VALUE:
      return "CHANNEL_SET_VALUE";
    case SUPLA_SD_CALL_CHANNELGROUP_SET_VALUE:
      return "CHANNELGROUP_SET_VALUE";
    case SUPLA_DS_CALL_CHANNEL_SET_VALUE_RESULT:
      return "CHANNEL_SET_VALUE_RESULT";
    case SUPLA_DS_CALL_DEVICE_CHANNEL_VALUE_CHANGED:
      return "DEVICE_CHANNEL_VALUE_CHANGED";
    case SUPLA_DS_CALL_DEVICE_CHANNEL_VALUE_CHANGED_B:
      return "DEVICE_CHANNEL_VALUE_CHANGED_B";
    case SUPLA_DS_CALL_DEVICE_CHANNEL_VALUE_CHANGED_C:
      return "VALUE_CHANGED_C";
    case SUPLA_DS_CALL_DEVICE_CHANNEL_EXTENDEDVALUE_CHANGED:
      return "DEVICE_CHANNEL_EXTENDEDVALUE_CHANGED";
    case SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST:
      return "DEVICE_CALCFG_REQUEST";
    case SUPLA_DS_CALL_DEVICE_CALCFG_RESULT:
      return "DEVICE_CALCFG_RESULT";
    case SUPLA_DS_CALL_GET_CHANNEL_FUNCTIONS:
      return "GET_CHANNEL_FUNCTIONS";
    case SUPLA_SD_CALL_GET_CHANNEL_FUNCTIONS_RESULT:
      return "GET_CHANNEL_FUNCTIONS_RESULT";
    case SUPLA_SD_CALL_CHANNEL_CONFIG_FINISHED:
      return "CHANNEL_CONFIG_FINISHED";
    case SUPLA_DS_CALL_ACTIONTRIGGER:
      return "ACTIONTRIGGER";
    case SUPLA_DS_CALL_REGISTER_PUSH_NOTIFICATION:
      return "REGISTER_PUSH_NOTIFICATION";
    case SUPLA_DS_CALL_SEND_PUSH_NOTIFICATION:
      return "SEND_PUSH_NOTIFICATION";
    case SUPLA_DS_CALL_SET_SUBDEVICE_DETAILS:
      return "SET_SUBDEVICE_DETAILS";
    default:
      break;
  }
  return "UNKNOWN";
}

const char *channelConfigTypeName(unsigned char configType) {
  switch (configType) {
    case SUPLA_CONFIG_TYPE_DEFAULT:
      return "default";
    case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE:
      return "weekly_schedule";
    case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE:
      return "alt_weekly_schedule";
    case SUPLA_CONFIG_TYPE_OCR:
      return "ocr";
    case SUPLA_CONFIG_TYPE_EXTENDED:
      return "extended";
    default:
      return "unknown";
  }
}

bool shouldDumpRawChannelConfig(const TSD_ChannelConfig *request) {
  if (request == nullptr) {
    return false;
  }

  switch (request->Func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
      return request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
             request->ConfigSize == sizeof(TChannelConfig_RollerShutter);
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
    case SUPLA_CHANNELFNC_VERTICAL_BLIND:
      return request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
             request->ConfigSize == sizeof(TChannelConfig_FacadeBlind);
    case SUPLA_CHANNELFNC_STAIRCASETIMER:
      return ((request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
               request->ConfigSize == sizeof(TChannelConfig_StaircaseTimer)) ||
              (request->ConfigType == SUPLA_CONFIG_TYPE_EXTENDED &&
               request->ConfigSize == sizeof(TChannelConfig_PowerSwitch)));
    case SUPLA_CHANNELFNC_POWERSWITCH:
    case SUPLA_CHANNELFNC_LIGHTSWITCH:
      return (request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT ||
              request->ConfigType == SUPLA_CONFIG_TYPE_EXTENDED) &&
             request->ConfigSize == sizeof(TChannelConfig_PowerSwitch);
    case SUPLA_CHANNELFNC_VALVE_OPENCLOSE:
    case SUPLA_CHANNELFNC_VALVE_PERCENTAGE:
      return request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
             request->ConfigSize == sizeof(TChannelConfig_Valve);
    case SUPLA_CHANNELFNC_ACTIONTRIGGER:
      return request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
             request->ConfigSize == sizeof(TChannelConfig_ActionTrigger);
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT:
      return (request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
              request->ConfigSize == sizeof(TChannelConfig_HVAC)) ||
             ((request->ConfigType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE ||
               request->ConfigType == SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE) &&
              request->ConfigSize == sizeof(TChannelConfig_WeeklySchedule));
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL:
    case SUPLA_CHANNELFNC_HVAC_DRYER:
    case SUPLA_CHANNELFNC_HVAC_FAN:
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL:
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER:
      return request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
             request->ConfigSize == sizeof(TChannelConfig_HVAC);
    case SUPLA_CHANNELFNC_BINARY_SENSOR:
    case SUPLA_CHANNELFNC_MOTION_SENSOR:
    case SUPLA_CHANNELFNC_FLOOD_SENSOR:
    case SUPLA_CHANNELFNC_CONTAINER_LEVEL_SENSOR:
    case SUPLA_CHANNELFNC_OPENINGSENSOR_GATEWAY:
    case SUPLA_CHANNELFNC_OPENINGSENSOR_GATE:
    case SUPLA_CHANNELFNC_OPENINGSENSOR_GARAGEDOOR:
    case SUPLA_CHANNELFNC_NOLIQUIDSENSOR:
    case SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR:
    case SUPLA_CHANNELFNC_OPENINGSENSOR_ROLLERSHUTTER:
    case SUPLA_CHANNELFNC_OPENINGSENSOR_ROOFWINDOW:
    case SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW:
    case SUPLA_CHANNELFNC_HOTELCARDSENSOR:
    case SUPLA_CHANNELFNC_ALARMARMAMENTSENSOR:
    case SUPLA_CHANNELFNC_MAILSENSOR:
    case SUPLA_CHANNELFNC_WINDSENSOR:
      return request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
             request->ConfigSize == sizeof(TChannelConfig_BinarySensor);
    case SUPLA_CHANNELFNC_THERMOMETER:
    case SUPLA_CHANNELFNC_HUMIDITY:
    case SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE:
      return request->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
             request->ConfigSize ==
                 sizeof(TChannelConfig_TemperatureAndHumidity);
    default:
      return false;
  }
}

bool isRegisterDeviceChunkedCall(int callId) {
  return callId == SUPLA_DS_CALL_REGISTER_DEVICE_F ||
         callId == SUPLA_DS_CALL_REGISTER_DEVICE_G;
}

bool isVerbosePacket(int callId) {
  switch (callId) {
    case SUPLA_DCS_CALL_PING_SERVER:
    case SUPLA_SDC_CALL_PING_SERVER_RESULT:
    case SUPLA_DS_CALL_DEVICE_CHANNEL_VALUE_CHANGED_C:
      return true;
    default:
      return false;
  }
}

bool looksLikeRegisterDeviceHeader(int callId,
                                   const uint8_t *buf,
                                   size_t size) {
  const size_t expected_size = sizeof(TSuplaDataPacket) - SUPLA_MAX_DATA_SIZE +
                               sizeof(TDS_SuplaRegisterDeviceHeader);
  if (!isRegisterDeviceChunkedCall(callId) || buf == nullptr ||
      size != expected_size) {
    return false;
  }

  const auto *packet = reinterpret_cast<const TSuplaDataPacket *>(buf);
  if (packet->call_id != static_cast<unsigned _supla_int_t>(callId)) {
    return false;
  }

  return packet->data_size > sizeof(TDS_SuplaRegisterDeviceHeader);
}

bool looksLikeRegisterDeviceChannelChunk(int callId,
                                         const uint8_t *buf,
                                         size_t size) {
  if (buf == nullptr) {
    return false;
  }

  return (callId == SUPLA_DS_CALL_REGISTER_DEVICE_F &&
          size == sizeof(TDS_SuplaDeviceChannel_D)) ||
         (callId == SUPLA_DS_CALL_REGISTER_DEVICE_G &&
          size == sizeof(TDS_SuplaDeviceChannel_E));
}

const char *registerDeviceChannelFuncFieldName(int callId, int channelType) {
  if (channelType == SUPLA_CHANNELTYPE_ACTIONTRIGGER) {
    return "ActionTriggerCaps";
  }

  if (callId == SUPLA_DS_CALL_REGISTER_DEVICE_G &&
      (channelType == SUPLA_CHANNELTYPE_DIMMER ||
       channelType == SUPLA_CHANNELTYPE_RGBLEDCONTROLLER ||
       channelType == SUPLA_CHANNELTYPE_DIMMERANDRGBLED)) {
    return "RGBW_FuncList";
  }

  return "FuncList";
}

#if __cplusplus >= 201703L
[[maybe_unused]]
#endif
const char *offlineFlagName(unsigned char offline) {
  switch (offline) {
    case SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE:
      return "online";
    case SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE:
      return "offline";
    case SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE:
      return "online (not available)";
    case SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE_REMOTE_WAKEUP_NOT_SUPPORTED:
      return "offline (remote wakeup not supported)";
    case SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING:
      return "firmware update ongoing";
    default:
      return "UNKNOWN";
  }
}

void formatChannelValueHex(const uint8_t *value, char *buffer, size_t size) {
  if (buffer == nullptr || size == 0) {
    return;
  }

  buffer[0] = '\0';
  generateHexString(value, buffer, SUPLA_CHANNELVALUE_SIZE, ' ');
}

void escapeLogString(const char *input,
                     size_t inputSize,
                     char *output,
                     size_t outputSize) {
  if (output == nullptr || outputSize == 0) {
    return;
  }

  output[0] = '\0';
  if (input == nullptr || inputSize == 0) {
    return;
  }

  size_t outPos = 0;
  for (size_t i = 0; i < inputSize && input[i] != '\0'; i++) {
    unsigned char ch = static_cast<unsigned char>(input[i]);
    const char *replacement = nullptr;
    char hex[5] = {};

    switch (ch) {
      case '\\':
        replacement = "\\\\";
        break;
      case '"':
        replacement = "\\\"";
        break;
      case '\n':
        replacement = "\\n";
        break;
      case '\r':
        replacement = "\\r";
        break;
      case '\t':
        replacement = "\\t";
        break;
      default:
        if (ch < 0x20 || ch > 0x7E) {
          snprintf(hex, sizeof(hex), "\\x%02X", ch);
          replacement = hex;
        }
        break;
    }

    const char *append = replacement;
    size_t appendLen = replacement != nullptr ? strlen(replacement) : 1;
    if (replacement == nullptr) {
      hex[0] = static_cast<char>(ch);
      hex[1] = '\0';
      append = hex;
      appendLen = 1;
    }

    if (outPos + appendLen >= outputSize) {
      const char *truncated = "...";
      size_t truncatedLen = strlen(truncated);
      if (outPos + truncatedLen < outputSize) {
        memcpy(output + outPos, truncated, truncatedLen);
        outPos += truncatedLen;
      }
      break;
    }
    memcpy(output + outPos, append, appendLen);
    outPos += appendLen;
  }

  output[outPos] = '\0';
}

bool appendLogToken(char *buffer,
                    size_t bufferSize,
                    size_t *pos,
                    const char *token) {
  if (buffer == nullptr || pos == nullptr || token == nullptr) {
    return false;
  }

  size_t tokenLen = strlen(token);
  if (*pos + tokenLen >= bufferSize) {
    return false;
  }
  memcpy(buffer + *pos, token, tokenLen);
  *pos += tokenLen;
  buffer[*pos] = '\0';
  return true;
}

void logRegisterDeviceHeader(int callId,
                             const uint8_t *buf,
                             size_t size,
                             const char *direction,
                             const char *callName) {
  const auto *packet = reinterpret_cast<const TSuplaDataPacket *>(buf);
  TDS_SuplaRegisterDeviceHeader header = {};
  memcpy(&header, packet->data, sizeof(header));
  char escapedName[96] = {};
  char escapedSoftVer[32] = {};
  char escapedServerName[96] = {};
  escapeLogString(
      header.Name, sizeof(header.Name), escapedName, sizeof(escapedName));
  escapeLogString(header.SoftVer,
                  sizeof(header.SoftVer),
                  escapedSoftVer,
                  sizeof(escapedSoftVer));
  escapeLogString(header.ServerName,
                  sizeof(header.ServerName),
                  escapedServerName,
                  sizeof(escapedServerName));

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) size=%zu payload={Email=<redacted>, "
      "AuthKey=<redacted>, GUID=<redacted>, Name=\"%s\", SoftVer=\"%s\", "
      "ServerName=\"%s\", Flags=0x%" PRIX32
      ", ManufacturerID=%d, ProductID=%d, "
      "channel_count=%u}",
      direction,
      callName,
      callId,
      size,
      escapedName,
      escapedSoftVer,
      escapedServerName,
      static_cast<uint32_t>(header.Flags),
      static_cast<int16_t>(header.ManufacturerID),
      static_cast<int16_t>(header.ProductID),
      static_cast<unsigned int>(header.channel_count));
}

void logSubdeviceDetails(int callId,
                         const TDS_SubdeviceDetails *details,
                         const char *direction,
                         const char *callName,
                         size_t size) {
  char escapedName[96] = {};
  char escapedSoftVer[32] = {};
  char escapedProductCode[64] = {};
  char escapedSerialNumber[64] = {};
  escapeLogString(
      details->Name, sizeof(details->Name), escapedName, sizeof(escapedName));
  escapeLogString(details->SoftVer,
                  sizeof(details->SoftVer),
                  escapedSoftVer,
                  sizeof(escapedSoftVer));
  escapeLogString(details->ProductCode,
                  sizeof(details->ProductCode),
                  escapedProductCode,
                  sizeof(escapedProductCode));
  escapeLogString(details->SerialNumber,
                  sizeof(details->SerialNumber),
                  escapedSerialNumber,
                  sizeof(escapedSerialNumber));

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) size=%zu payload={SubDeviceId=%u, Name=\"%s\", "
      "SoftVer=\"%s\", ProductCode=\"%s\", SerialNumber=\"%s\"}",
      direction,
      callName,
      callId,
      size,
      static_cast<unsigned int>(details->SubDeviceId),
      escapedName,
      escapedSoftVer,
      escapedProductCode,
      escapedSerialNumber);
}

void logSetChannelCaption(int callId,
                          const TDCS_SetCaption *caption,
                          const char *direction,
                          const char *callName,
                          size_t size) {
  char escapedCaption[128] = {};
  escapeLogString(caption->Caption,
                  caption->CaptionSize > 0 ? caption->CaptionSize - 1 : 0,
                  escapedCaption,
                  sizeof(escapedCaption));

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) size=%zu payload={ChannelNumber=%u, "
      "CaptionSize=%" PRIu32 ", Caption=\"%s\"}",
      direction,
      callName,
      callId,
      size,
      static_cast<unsigned int>(caption->ChannelNumber),
      static_cast<uint32_t>(caption->CaptionSize),
      escapedCaption);
}

void logSetChannelCaptionResult(int callId,
                                const TSCD_SetCaptionResult *caption,
                                const char *direction,
                                const char *callName,
                                size_t size) {
  char escapedCaption[128] = {};
  escapeLogString(caption->Caption,
                  caption->CaptionSize > 0 ? caption->CaptionSize - 1 : 0,
                  escapedCaption,
                  sizeof(escapedCaption));

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) size=%zu payload={ChannelNumber=%u, "
      "ResultCode=%u, CaptionSize=%" PRIu32 ", Caption=\"%s\"}",
      direction,
      callName,
      callId,
      size,
      static_cast<unsigned int>(caption->ChannelNumber),
      static_cast<unsigned int>(caption->ResultCode),
      static_cast<uint32_t>(caption->CaptionSize),
      escapedCaption);
}

void logDeviceCalCfgRequest(int callId,
                            const TSD_DeviceCalCfgRequest *request,
                            const char *direction,
                            const char *callName,
                            size_t size) {
  if (request == nullptr) {
    return;
  }

  if (request->Command == SUPLA_CALCFG_CMD_SET_CFG_MODE_PASSWORD) {
    SUPLA_LOG_DEBUG(
        "SRPC %s call=%s(%d) size=%zu payload={SenderID=%" PRId32
        ", ChannelNumber=%" PRId32 ", Command=%" PRId32
        ", SuperUserAuthorized=%u, DataType=%" PRId32 ", DataSize=%" PRIu32
        ", Data=<redacted>}",
        direction,
        callName,
        callId,
        size,
        static_cast<int32_t>(request->SenderID),
        static_cast<int32_t>(request->ChannelNumber),
        static_cast<int32_t>(request->Command),
        static_cast<unsigned int>(request->SuperUserAuthorized),
        static_cast<int32_t>(request->DataType),
        static_cast<uint32_t>(request->DataSize));
    return;
  }

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) size=%zu payload={SenderID=%" PRId32
      ", ChannelNumber=%" PRId32 ", Command=%" PRId32
      ", SuperUserAuthorized=%u, DataType=%" PRId32 ", DataSize=%" PRIu32
      "}",
      direction,
      callName,
      callId,
      size,
      static_cast<int32_t>(request->SenderID),
      static_cast<int32_t>(request->ChannelNumber),
      static_cast<int32_t>(request->Command),
      static_cast<unsigned int>(request->SuperUserAuthorized),
      static_cast<int32_t>(request->DataType),
      static_cast<uint32_t>(request->DataSize));

  const size_t headerSize = offsetof(TSD_DeviceCalCfgRequest, Data);
  const size_t dataSize = static_cast<size_t>(request->DataSize);
  if (dataSize == 0) {
    SUPLA_LOG_DEBUG("SRPC %s call=%s(%d) payload={Data=<empty>}",
                    direction,
                    callName,
                    callId);
    return;
  }
  if (dataSize > SUPLA_CALCFG_DATA_MAXSIZE) {
    SUPLA_LOG_DEBUG(
        "SRPC %s call=%s(%d) payload={Data=<invalid-size>, DataSize=%zu}",
        direction,
        callName,
        callId,
        dataSize);
    return;
  }
  if (size < headerSize || dataSize > size - headerSize) {
    const size_t availableSize = size > headerSize ? size - headerSize : 0;
    (void)(availableSize);
    SUPLA_LOG_DEBUG(
        "SRPC %s call=%s(%d) payload={Data=<truncated>, DataSize=%zu, "
        "Available=%zu}",
        direction,
        callName,
        callId,
        dataSize,
        availableSize);
    return;
  }

  logRawHexDump(direction,
                callId,
                callName,
                reinterpret_cast<const uint8_t *>(request->Data),
                dataSize,
                false);
}

void logChannelConfigRawPayload(int callId,
                                const TSD_ChannelConfig *request,
                                const char *direction,
                                const char *callName) {
  logRawHexDump(direction,
                callId,
                callName,
                reinterpret_cast<const uint8_t *>(request->Config),
                request->ConfigSize,
                false);
}

void logChannelConfigOcrPayload(int callId,
                                const TSD_ChannelConfig *request,
                                const char *direction,
                                const char *callName) {
  if (request->ConfigSize < sizeof(TChannelConfig_OCR)) {
    SUPLA_LOG_DEBUG(
        "SRPC %s call=%s(%d) payload={ChannelNumber=%u, Func=%" PRId32 ", "
        "ConfigType=%u(%s), ConfigSize=%u, OCR=<truncated>}",
        direction,
        callName,
        callId,
        static_cast<unsigned int>(request->ChannelNumber),
        static_cast<int32_t>(request->Func),
        static_cast<unsigned int>(request->ConfigType),
        channelConfigTypeName(request->ConfigType),
        static_cast<unsigned int>(request->ConfigSize));
    return;
  }

  TChannelConfig_OCR ocrConfig = {};
  memcpy(&ocrConfig, request->Config, sizeof(ocrConfig));
  ocrConfig.AuthKey[sizeof(ocrConfig.AuthKey) - 1] = '\0';
  ocrConfig.Host[sizeof(ocrConfig.Host) - 1] = '\0';

  char escapedHost[128] = {};
  escapeLogString(
      ocrConfig.Host, sizeof(ocrConfig.Host), escapedHost, sizeof(escapedHost));

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) payload={ChannelNumber=%u, Func=%" PRId32 ", "
      "ConfigType=%u(%s), ConfigSize=%u, OCR={AuthKey=<redacted>, "
      "Host=\"%s\", PhotoIntervalSec=%" PRIu32 ", LightingMode=0x%" PRIX32
      "%08" PRIX32 ", LightingLevel=%u, MaximumIncrement=0x%" PRIX32
      "%08" PRIX32 ", AvailableLightingModes=0x%" PRIX32 "%08" PRIX32 "}}",
      direction,
      callName,
      callId,
      static_cast<unsigned int>(request->ChannelNumber),
      static_cast<int32_t>(request->Func),
      static_cast<unsigned int>(request->ConfigType),
      channelConfigTypeName(request->ConfigType),
      static_cast<unsigned int>(request->ConfigSize),
      escapedHost,
      static_cast<uint32_t>(ocrConfig.PhotoIntervalSec),
      PRINTF_UINT64_HEX(ocrConfig.LightingMode),
      static_cast<unsigned int>(ocrConfig.LightingLevel),
      PRINTF_UINT64_HEX(ocrConfig.MaximumIncrement),
      PRINTF_UINT64_HEX(ocrConfig.AvailableLightingModes));
}

void logSetChannelConfigRequest(int callId,
                                const TSD_ChannelConfig *request,
                                const char *direction,
                                const char *callName,
                                size_t size) {
  if (request == nullptr) {
    return;
  }

  const size_t headerSize = offsetof(TSD_ChannelConfig, Config);
  const size_t configSize = static_cast<size_t>(request->ConfigSize);
  const char *configTypeName = channelConfigTypeName(request->ConfigType);
  (void)configTypeName;

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) size=%zu payload={ChannelNumber=%u, Func=%" PRId32
      ", "
      "ConfigType=%u(%s), ConfigSize=%u}",
      direction,
      callName,
      callId,
      size,
      static_cast<unsigned int>(request->ChannelNumber),
      static_cast<int32_t>(request->Func),
      static_cast<unsigned int>(request->ConfigType),
      configTypeName,
      static_cast<unsigned int>(request->ConfigSize));

  if (configSize == 0) {
    SUPLA_LOG_DEBUG("SRPC %s call=%s(%d) payload={Config=<empty>}",
                    direction,
                    callName,
                    callId);
    return;
  }

  if (size < headerSize + configSize) {
    SUPLA_LOG_DEBUG(
        "SRPC %s call=%s(%d) payload={Config=<truncated>, expected=%zu}",
        direction,
        callName,
        callId,
        headerSize + configSize);
    return;
  }

  switch (request->ConfigType) {
    case SUPLA_CONFIG_TYPE_OCR:
      logChannelConfigOcrPayload(callId, request, direction, callName);
      return;
    case SUPLA_CONFIG_TYPE_DEFAULT:
    case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE:
    case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE:
    case SUPLA_CONFIG_TYPE_EXTENDED:
      logChannelConfigRawPayload(callId, request, direction, callName);
      return;
    default:
      SUPLA_LOG_DEBUG("SRPC %s call=%s(%d) payload={Config=<unsupported type>}",
                      direction,
                      callName,
                      callId);
      return;
  }
}

void logRegisterDeviceChannelChunk(int callId,
                                   const TDS_SuplaDeviceChannel_D *channel,
                                   const char *direction,
                                   const char *callName,
                                   size_t size) {
  char valueHex[3 * SUPLA_CHANNELVALUE_SIZE] = {};
  formatChannelValueHex(reinterpret_cast<const uint8_t *>(channel->value),
                        valueHex,
                        sizeof(valueHex));

  const char *funcFieldName =
      registerDeviceChannelFuncFieldName(callId, channel->Type);
  (void)funcFieldName;
  uint32_t funcFieldValue =
      (channel->Type == SUPLA_CHANNELTYPE_ACTIONTRIGGER)
          ? static_cast<uint32_t>(channel->ActionTriggerCaps)
          : static_cast<uint32_t>(channel->FuncList);
  (void)funcFieldValue;

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) size=%zu payload={Number=%u, Type=%" PRId32
      ", %s=0x%" PRIX32 ", Default=%" PRId32 ", Flags=0x%" PRIX32 "%08" PRIX32
      ", Offline=%u(%s), ValueValidityTimeSec=%" PRIu32 ", "
      "value[8]=%s, DefaultIcon=%u}",
      direction,
      callName,
      callId,
      size,
      static_cast<unsigned int>(channel->Number),
      static_cast<int32_t>(channel->Type),
      funcFieldName,
      funcFieldValue,
      static_cast<int32_t>(channel->Default),
      PRINTF_UINT64_HEX(static_cast<uint64_t>(channel->Flags)),
      static_cast<unsigned int>(channel->Offline),
      offlineFlagName(channel->Offline),
      static_cast<uint32_t>(channel->ValueValidityTimeSec),
      valueHex,
      static_cast<unsigned int>(channel->DefaultIcon));
}

void logRegisterDeviceChannelChunk(int callId,
                                   const TDS_SuplaDeviceChannel_E *channel,
                                   const char *direction,
                                   const char *callName,
                                   size_t size) {
  char valueHex[3 * SUPLA_CHANNELVALUE_SIZE] = {};
  formatChannelValueHex(reinterpret_cast<const uint8_t *>(channel->value),
                        valueHex,
                        sizeof(valueHex));

  const char *funcFieldName =
      registerDeviceChannelFuncFieldName(callId, channel->Type);
  (void)funcFieldName;
  uint32_t funcFieldValue = 0;
  (void)funcFieldValue;
  if (channel->Type == SUPLA_CHANNELTYPE_ACTIONTRIGGER) {
    funcFieldValue = channel->ActionTriggerCaps;
  } else if (callId == SUPLA_DS_CALL_REGISTER_DEVICE_G &&
             (channel->Type == SUPLA_CHANNELTYPE_DIMMER ||
              channel->Type == SUPLA_CHANNELTYPE_RGBLEDCONTROLLER ||
              channel->Type == SUPLA_CHANNELTYPE_DIMMERANDRGBLED)) {
    funcFieldValue = channel->RGBW_FuncList;
  } else {
    funcFieldValue = channel->FuncList;
  }

  SUPLA_LOG_DEBUG(
      "SRPC %s call=%s(%d) size=%zu payload={Number=%u, Type=%" PRId32
      ", %s=0x%" PRIX32 ", Default=%" PRId32 ", Flags=0x%" PRIX32 "%08" PRIX32
      ", Offline=%u(%s), ValueValidityTimeSec=%" PRIu32 ", "
      "value[8]=%s, DefaultIcon=%u, SubDeviceId=%u}",
      direction,
      callName,
      callId,
      size,
      static_cast<unsigned int>(channel->Number),
      static_cast<int32_t>(channel->Type),
      funcFieldName,
      funcFieldValue,
      static_cast<int32_t>(channel->Default),
      PRINTF_UINT64_HEX(static_cast<uint64_t>(channel->Flags)),
      static_cast<unsigned int>(channel->Offline),
      offlineFlagName(channel->Offline),
      static_cast<uint32_t>(channel->ValueValidityTimeSec),
      valueHex,
      static_cast<unsigned int>(channel->DefaultIcon),
      static_cast<unsigned int>(channel->SubDeviceId));
}

void logDeviceChannelValueChanged(int callId,
                                  const TDS_SuplaDeviceChannelValue_C *value,
                                  const char *direction,
                                  const char *callName,
                                  size_t size) {
  char valueHex[3 * SUPLA_CHANNELVALUE_SIZE] = {};
  formatChannelValueHex(reinterpret_cast<const uint8_t *>(value->value),
                        valueHex,
                        sizeof(valueHex));

  SUPLA_LOG_VERBOSE(
      "SRPC %s call=%s(%d) size=%zu payload={ChannelNumber=%u, Offline=%u, "
      "ValidityTimeSec=%" PRIu32 ", value[8]=%s}",
      direction,
      callName,
      callId,
      size,
      static_cast<unsigned int>(value->ChannelNumber),
      static_cast<unsigned int>(value->Offline),
      static_cast<uint32_t>(value->ValidityTimeSec),
      valueHex);
}

void logRawHexDump(const char *direction,
                   int callId,
                   const char *callName,
                   const uint8_t *buf,
                   size_t size,
                   bool verbose) {
  const size_t rleThreshold = 6;
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266) || defined(__AVR__) || \
    defined(ARDUINO_ARCH_AVR)
  char tmp[256] = {};
#else
  char tmp[2048] = {};
#endif
  size_t pos = 0;
  bool truncated = false;
  size_t i = 0;
  for (; i < size;) {
    if (pos + 4 >= sizeof(tmp)) {
      truncated = true;
      break;
    }

    size_t run = 1;
    while (i + run < size && buf[i + run] == buf[i]) {
      run++;
    }

    if (run >= rleThreshold) {
      char runToken[32] = {};
      snprintf(runToken, sizeof(runToken), "%02X{%zu} ", buf[i], run);
      if (!appendLogToken(tmp, sizeof(tmp), &pos, runToken)) {
        truncated = true;
        break;
      }
      i += run;
      continue;
    }

    char byteToken[4] = {};
    snprintf(byteToken, sizeof(byteToken), "%02X ", buf[i]);
    if (!appendLogToken(tmp, sizeof(tmp), &pos, byteToken)) {
      truncated = true;
      break;
    }
    i++;
  }

  if (i < size) {
    truncated = true;
  }

  if (truncated) {
    appendLogToken(tmp, sizeof(tmp), &pos, "...");
  }

  if (verbose) {
    SUPLA_LOG_VERBOSE("SRPC %s call=%s(%d) size=%zu raw=[%s]",
                      direction,
                      callName,
                      callId,
                      size,
                      tmp);
  } else {
    SUPLA_LOG_DEBUG("SRPC %s call=%s(%d) size=%zu raw=[%s]",
                    direction,
                    callName,
                    callId,
                    size,
                    tmp);
  }
}

}  // namespace

Supla::Protocol::SuplaSrpc::SuplaSrpc(SuplaDeviceClass *sdc, int version)
    : Supla::Protocol::ProtocolLayer(sdc), version(version) {
  setSuplaCACert(::suplaCACert);
  setSupla3rdPartyCACert(::supla3rdCACert);
}

void Supla::Protocol::SuplaSrpc::onPacketSent(void *userParam,
                                              unsigned _supla_int_t callId,
                                              void *data,
                                              unsigned _supla_int_t dataSize,
                                              void *reserved) {
  (void)(reserved);
  auto *self = reinterpret_cast<SuplaSrpc *>(userParam);
  if (self == nullptr) {
    return;
  }

  self->logSrpcPacket(true,
                      static_cast<int>(callId),
                      reinterpret_cast<const uint8_t *>(data),
                      dataSize);
}

void Supla::Protocol::SuplaSrpc::onPacketReceived(void *userParam,
                                                  unsigned _supla_int_t callId,
                                                  void *data,
                                                  unsigned _supla_int_t
                                                      dataSize,
                                                  void *reserved) {
  (void)(reserved);
  auto *self = reinterpret_cast<SuplaSrpc *>(userParam);
  if (self == nullptr) {
    return;
  }

  self->logSrpcPacket(false,
                      static_cast<int>(callId),
                      reinterpret_cast<const uint8_t *>(data),
                      dataSize);
}

void Supla::Protocol::SuplaSrpc::logSrpcPacket(bool send,
                                               int callId,
                                               const uint8_t *buf,
                                               size_t size) {
  bool verbose = isVerbosePacket(callId);
  int requiredLogLevel = verbose ? LOG_VERBOSE : LOG_DEBUG;

  if (!supla_log_is_enabled(requiredLogLevel)) {
    return;
  }

  const char *direction = send ? "TX" : "RX";
  const char *callName = callIdToName(callId);

  if (looksLikeRegisterDeviceHeader(callId, buf, size)) {
    logRegisterDeviceHeader(callId, buf, size, direction, callName);
    return;
  }

  if (looksLikeRegisterDeviceChannelChunk(callId, buf, size)) {
    if (callId == SUPLA_DS_CALL_REGISTER_DEVICE_F) {
      logRegisterDeviceChannelChunk(
          callId,
          reinterpret_cast<const TDS_SuplaDeviceChannel_D *>(buf),
          direction,
          callName,
          size);
    } else {
      logRegisterDeviceChannelChunk(
          callId,
          reinterpret_cast<const TDS_SuplaDeviceChannel_E *>(buf),
          direction,
          callName,
          size);
    }
    return;
  }

  if (callId == SUPLA_DS_CALL_SET_SUBDEVICE_DETAILS &&
      size == sizeof(TDS_SubdeviceDetails)) {
    logSubdeviceDetails(callId,
                        reinterpret_cast<const TDS_SubdeviceDetails *>(buf),
                        direction,
                        callName,
                        size);
    return;
  }

  if (callId == SUPLA_SD_CALL_SET_CHANNEL_CONFIG ||
      callId == SUPLA_DS_CALL_SET_CHANNEL_CONFIG) {
    if (size >= offsetof(TSD_ChannelConfig, Config)) {
      auto *request = reinterpret_cast<const TSD_ChannelConfig *>(buf);
      if (shouldDumpRawChannelConfig(request)) {
        logSetChannelConfigRequest(callId, request, direction, callName, size);
      } else {
        SUPLA_LOG_DEBUG(
            "SRPC %s call=%s(%d) size=%zu payload={ChannelNumber=%u, "
            "Func=%" PRId32
            ", ConfigType=%u(%s), ConfigSize=%u, Config=<redacted>}",
            direction,
            callName,
            callId,
            size,
            static_cast<unsigned int>(request->ChannelNumber),
            static_cast<int32_t>(request->Func),
            static_cast<unsigned int>(request->ConfigType),
            channelConfigTypeName(request->ConfigType),
            static_cast<unsigned int>(request->ConfigSize));
      }
      return;
    }
  }

  if (callId == SUPLA_DCS_CALL_SET_CHANNEL_CAPTION &&
      size >= offsetof(TDCS_SetCaption, Caption)) {
    auto *caption = reinterpret_cast<const TDCS_SetCaption *>(buf);
    size_t expectedSize =
        offsetof(TDCS_SetCaption, Caption) + caption->CaptionSize;
    if (caption->CaptionSize > 0 &&
        caption->CaptionSize <= SUPLA_CAPTION_MAXSIZE && size == expectedSize) {
      logSetChannelCaption(callId, caption, direction, callName, size);
      return;
    }
  }

  if (callId == SUPLA_SCD_CALL_SET_CHANNEL_CAPTION_RESULT &&
      size >= offsetof(TSCD_SetCaptionResult, Caption)) {
    auto *caption = reinterpret_cast<const TSCD_SetCaptionResult *>(buf);
    size_t expectedSize =
        offsetof(TSCD_SetCaptionResult, Caption) + caption->CaptionSize;
    if (caption->CaptionSize > 0 &&
        caption->CaptionSize <= SUPLA_CAPTION_MAXSIZE && size == expectedSize) {
      logSetChannelCaptionResult(callId, caption, direction, callName, size);
      return;
    }
  }

  if (callId == SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST &&
      size >= offsetof(TSD_DeviceCalCfgRequest, Data)) {
    auto *request = reinterpret_cast<const TSD_DeviceCalCfgRequest *>(buf);
    logDeviceCalCfgRequest(callId, request, direction, callName, size);
    return;
  }

  if (callId == SUPLA_DS_CALL_DEVICE_CHANNEL_VALUE_CHANGED_C &&
      size == sizeof(TDS_SuplaDeviceChannelValue_C)) {
    logDeviceChannelValueChanged(
        callId,
        reinterpret_cast<const TDS_SuplaDeviceChannelValue_C *>(buf),
        direction,
        callName,
        size);
    return;
  }

  if (callId == SUPLA_DCS_CALL_PING_SERVER) {
    SUPLA_LOG_DEBUG("SRPC %s call=%s(%d) size=%zu", direction, callName, callId,
                    size);
    return;
  }

  if (callId == SUPLA_SDC_CALL_PING_SERVER_RESULT) {
    SUPLA_LOG_DEBUG("SRPC %s call=%s(%d) size=%zu", direction, callName, callId,
                    size);
    return;
  }

  if (isSensitiveCallId(callId) || buf == nullptr || size == 0) {
    SUPLA_LOG_DEBUG("SRPC %s call=%s(%d) size=%zu payload=<redacted>",
                    direction,
                    callName,
                    callId,
                    size);
    return;
  }

  logRawHexDump(direction, callId, callName, buf, size, verbose);
}

const char *Supla::Protocol::SuplaSrpc::callIdToName(int callId) {
  return safeCallName(callId);
}

bool Supla::Protocol::SuplaSrpc::isSensitiveCallId(int callId) {
  switch (callId) {
    case SUPLA_DS_CALL_REGISTER_DEVICE:
    case SUPLA_DS_CALL_REGISTER_DEVICE_B:
    case SUPLA_DS_CALL_REGISTER_DEVICE_C:
    case SUPLA_DS_CALL_REGISTER_DEVICE_D:
    case SUPLA_DS_CALL_REGISTER_DEVICE_E:
      return true;
    default:
      return false;
  }
}

Supla::Protocol::SuplaSrpc::~SuplaSrpc() {
  if (client) {
    delete client;
    client = nullptr;
  }
  if (selectedCertificate) {
    if (selectedCertificate != suplaCACert &&
        selectedCertificate != supla3rdPartyCACert &&
        selectedCertificate != wrongCert) {
      delete[] selectedCertificate;
    }
  }
}

void Supla::Protocol::SuplaSrpc::setNetworkClient(Supla::Client *newClient) {
  if (client) {
    delete client;
  }
  client = newClient;
  initClient();
}

void Supla::Protocol::SuplaSrpc::initClient() {
  if (client == nullptr) {
    if (Supla::Network::Instance()) {
      client = Supla::Network::Instance()->createClient();
    } else {
      client = Supla::ClientBuilder();
      if (client == nullptr) {
        SUPLA_LOG_ERROR("Failed to create client");
        return;
      }
    }
  }
  client->setSdc(sdc);
  client->setDebugLogs(verboseLog);
  if (port == 2016 || (port == -1 && isSuplaSSLEnabled)) {
    client->setSSLEnabled(true);
    client->setCACert(selectedCertificate);
  }
}

void Supla::Protocol::SuplaSrpc::setLowLevelDebugLogs(bool value) {
  if (client != nullptr) {
    client->setDebugLogs(value);
  }
}

bool Supla::Protocol::SuplaSrpc::onLoadConfig() {
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg == nullptr) {
    return false;
  }

  bool configComplete = true;
  char buf[256] = {};

  // Supla protocol specific config
  if (cfg->isSuplaCommProtocolEnabled()) {
    enabled = true;
    memset(buf, 0, sizeof(buf));
    if (cfg->getSuplaServer(buf) && strlen(buf) > 0) {
      sdc->setServer(buf);
      configEmpty = false;
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing server");
#ifdef ARDUINO_ARCH_AVR
      configComplete = false;
#endif  // ARDUINO_ARCH_AVR
    }
    setServerPort(cfg->getSuplaServerPort());

    memset(buf, 0, sizeof(buf));
    if (cfg->getEmail(buf) && strlen(buf) > 0) {
      sdc->setEmail(buf);
      configEmpty = false;
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing email");
      configComplete = false;
    }

    if (port == 2016 || (port == -1 && isSuplaSSLEnabled)) {
      bool usePublicServer =
          Supla::RegisterDevice::isSuplaPublicServerConfigured();
      selectedCertificate = suplaCACert;

      // Public Supla server use different root CA for server certificate
      // validation then CA used for private servers
      if (!usePublicServer) {
        selectedCertificate = supla3rdPartyCACert;
      }

      cfg->getUInt8("security_level", &securityLevel);
      if (securityLevel > 2) {
        securityLevel = 0;
      }

      SUPLA_LOG_DEBUG("Security level: %s",
                      securityLevel == 2   ? "Skip CA check"
                      : securityLevel == 1 ? "Custom CA"
                                           : "Supla CA");
      switch (securityLevel) {
        default:
        case 0: {
          // in case of default security level it is required to use Supla CA
          // certificate. It should be set on application level before
          // SuplaDevice.begin() is called.
          // If it is null, we just assign "SUPLA" as a certificate value, which
          // will of course fail the certificate validation (which is intended).
          if (selectedCertificate == nullptr) {
            SUPLA_LOG_ERROR(
                "Supla CA ceritificate is selected, but it is not set. "
                "Connection will fail");
            selectedCertificate = wrongCert;
          }
          break;
        }
        case 1: {
          // custom CA from Config
          int len = cfg->getCustomCASize();
          if (len > 0) {
            len++;
            auto cert = new char[len];
            memset(cert, 0, len);
            cfg->getCustomCA(cert, len);
            selectedCertificate = cert;
          } else {
            SUPLA_LOG_ERROR(
                "Custom CA is selected, but certificate is"
                " missing in config. Connect will fail");
            selectedCertificate = wrongCert;
          }
          break;
        }
        case 2: {
          // Skip certificate validation (INSECURE)
          selectedCertificate = nullptr;
          break;
        }
      }
    }
  } else {
    enabled = false;
  }

  return configComplete;
}

void Supla::Protocol::SuplaSrpc::onInit() {
  if (!isEnabled()) {
    return;
  }

  if (port == 2016 || (port == -1 && isSuplaSSLEnabled)) {
    auto cfg = Supla::Storage::ConfigInstance();

    if (!cfg && (suplaCACert != nullptr || supla3rdPartyCACert != nullptr)) {
      selectedCertificate = suplaCACert;
      if (suplaCACert != nullptr && supla3rdPartyCACert != nullptr) {
        bool usePublicServer =
            Supla::RegisterDevice::isSuplaPublicServerConfigured();

        // Public Supla server use different root CA for server certificate
        // validation then CA used for private servers
        if (!usePublicServer) {
          selectedCertificate = supla3rdPartyCACert;
        }
      }

      if (suplaCACert == nullptr) {
        selectedCertificate = supla3rdPartyCACert;
      }
    }
  }

  SUPLA_LOG_INFO("Using Supla protocol version %d", version);
}

_supla_int_t Supla::dataRead(void *buf, _supla_int_t count, void *userParams) {
  auto srpcLayer = reinterpret_cast<Supla::Protocol::SuplaSrpc *>(userParams);
  return srpcLayer->client->read(reinterpret_cast<uint8_t *>(buf), count);
}

_supla_int_t Supla::dataWrite(void *buf, _supla_int_t count, void *userParams) {
  auto srpcLayer = reinterpret_cast<Supla::Protocol::SuplaSrpc *>(userParams);
  _supla_int_t r =
      srpcLayer->client->write(reinterpret_cast<uint8_t *>(buf), count);
  if (r > 0) {
    srpcLayer->updateLastSentTime();
  }
  return r;
}

void Supla::messageReceived(void *srpc,
                            unsigned _supla_int_t rrId,
                            unsigned _supla_int_t callType,
                            void *userParam,
                            unsigned char protoVersion) {
  (void)(rrId);
  (void)(callType);
  (void)(protoVersion);
  TsrpcReceivedData rd;
  int8_t getDataResult;

  Supla::Protocol::SuplaSrpc *suplaSrpc =
      reinterpret_cast<Supla::Protocol::SuplaSrpc *>(userParam);

  suplaSrpc->updateLastResponseTime();

  if (SUPLA_RESULT_TRUE == (getDataResult = srpc_getdata(srpc, &rd, 0))) {
    switch (rd.call_id) {
      case SUPLA_SDC_CALL_VERSIONERROR:
        suplaSrpc->onVersionError(rd.data.sdc_version_error);
        break;
      case SUPLA_SD_CALL_REGISTER_DEVICE_RESULT:
        suplaSrpc->onRegisterResult(rd.data.sd_register_device_result);
        break;
      case SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B:
        suplaSrpc->onRegisterResultB(rd.data.sd_register_device_result_b);
        break;
      case SUPLA_SD_CALL_CHANNEL_SET_VALUE: {
        auto element = Supla::Element::getElementByChannelNumber(
            rd.data.sd_channel_new_value->ChannelNumber);
        if (element) {
          int actionResult =
              element->handleNewValueFromServer(rd.data.sd_channel_new_value);
          if (actionResult != -1) {
            srpc_ds_async_set_channel_result(
                srpc,
                rd.data.sd_channel_new_value->ChannelNumber,
                rd.data.sd_channel_new_value->SenderID,
                actionResult);
          }
        } else {
          SUPLA_LOG_WARNING(
              "Error: couldn't find element for a requested channel [%d]",
              rd.data.sd_channel_new_value->ChannelNumber);
        }
        break;
      }
      case SUPLA_SDC_CALL_SET_ACTIVITY_TIMEOUT_RESULT:
        suplaSrpc->onSetActivityTimeoutResult(
            rd.data.sdc_set_activity_timeout_result);
        break;
      case SUPLA_CSD_CALL_GET_CHANNEL_STATE: {
        suplaSrpc->sendChannelStateResult(
            rd.data.csd_channel_state_request->SenderID,
            rd.data.csd_channel_state_request->ChannelNumber);
        break;
      }
      case SUPLA_SDC_CALL_PING_SERVER_RESULT:
        break;

      case SUPLA_DCS_CALL_GET_USER_LOCALTIME_RESULT: {
        suplaSrpc->onGetUserLocaltimeResult(rd.data.sdc_user_localtime_result);
        break;
      }
      case SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST: {
        TDS_DeviceCalCfgResult result = {};
        result.ReceiverID = rd.data.sd_device_calcfg_request->SenderID;
        result.ChannelNumber = rd.data.sd_device_calcfg_request->ChannelNumber;
        result.Command = rd.data.sd_device_calcfg_request->Command;
        result.Result = SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
        result.DataSize = 0;
        SUPLA_LOG_DEBUG(
            "CALCFG CMD received: senderId %d, ch %d, cmd %d (0x%X), suauth "
            "%d, "
            "datatype %d, datasize %d",
            rd.data.sd_device_calcfg_request->SenderID,
            rd.data.sd_device_calcfg_request->ChannelNumber,
            rd.data.sd_device_calcfg_request->Command,
            rd.data.sd_device_calcfg_request->Command,
            rd.data.sd_device_calcfg_request->SuperUserAuthorized,
            rd.data.sd_device_calcfg_request->DataType,
            rd.data.sd_device_calcfg_request->DataSize);

        if (rd.data.sd_device_calcfg_request->ChannelNumber == -1) {
          // calcfg with channel == -1 are for whole device, so we route
          // it to SuplaDeviceClass instance
          result.Result = suplaSrpc->getSdc()->handleCalcfgFromServer(
              rd.data.sd_device_calcfg_request, &result);
          if ((result.Command == SUPLA_CALCFG_CMD_START_SUBDEVICE_PAIRING ||
               result.Command == SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE) &&
              result.Result == SUPLA_CALCFG_RESULT_TRUE) {
            suplaSrpc->calCfgResultPending.set(
                result.ChannelNumber, result.ReceiverID, result.Command, 0);
          }
        } else {
          auto element = Supla::Element::getElementByChannelNumber(
              rd.data.sd_device_calcfg_request->ChannelNumber);
          if (element) {
            // try to process calcfg by element responsible for channel
            result.Result = element->handleCalcfgFromServer(
                rd.data.sd_device_calcfg_request);
            if (result.Result == SUPLA_SRPC_CALCFG_RESULT_PENDING) {
              suplaSrpc->calCfgResultPending.set(
                  result.ChannelNumber,
                  result.ReceiverID,
                  result.Command,
                  element->getCalcfgPendingTimeoutMs(
                      rd.data.sd_device_calcfg_request));
            } else if (result.Result == SUPLA_CALCFG_RESULT_NOT_SUPPORTED) {
              // if request wasn't processed by channel, try to check if there
              // is element related to it's subdevice (if any)
              auto channel = element->getChannelByChannelNumber(
                  rd.data.sd_device_calcfg_request->ChannelNumber);
              auto subdevice = channel ? channel->getSubDeviceId() : 0;
              SUPLA_LOG_DEBUG("Trying to find subdevice (%d) for channel %d",
                              subdevice,
                              rd.data.sd_device_calcfg_request->ChannelNumber);
              if (subdevice > 0) {
                auto subdeviceElement =
                    Supla::Element::getOwnerOfSubDeviceId(subdevice);
                if (subdeviceElement) {
                  result.Result = subdeviceElement->handleCalcfgFromServer(
                      rd.data.sd_device_calcfg_request);
                } else {
                  SUPLA_LOG_WARNING("Error: couldn't find subdevice %d element",
                                    subdevice);
                }
              }
            }
          } else {
            SUPLA_LOG_WARNING(
                "Error: couldn't find element for a requested channel [%d]",
                rd.data.sd_channel_new_value->ChannelNumber);
          }
        }
        if (result.Result >= 0) {
          SUPLA_LOG_DEBUG("Sending CALCFG result: cmd %d (0x%X), result: %d",
                          result.Command,
                          result.Command,
                          result.Result);
          srpc_ds_async_device_calcfg_result(srpc, &result);
        }
        break;
      }
      case SUPLA_SD_CALL_GET_CHANNEL_CONFIG_RESULT: {
        TSD_ChannelConfig *result = rd.data.sd_channel_config;
        if (result) {
          auto element =
              Supla::Element::getElementByChannelNumber(result->ChannelNumber);
          if (element) {
            switch (result->ConfigType) {
              default:
              case SUPLA_CONFIG_TYPE_DEFAULT: {
                element->handleChannelConfig(result);
                break;
              }
              case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE: {
                element->handleWeeklySchedule(result);
                break;
              }
              case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE: {
                element->handleWeeklySchedule(result, true);
                break;
              }
            }
          } else {
            SUPLA_LOG_WARNING(
                "Error: couldn't find element for a requested channel [%d]",
                result->ChannelNumber);
          }
        }
        break;
      }
      case SUPLA_SD_CALL_SET_CHANNEL_CONFIG_RESULT: {
        auto *result = rd.data.sds_set_channel_config_result;
        if (result) {
          auto element =
              Supla::Element::getElementByChannelNumber(result->ChannelNumber);
          if (element) {
            element->handleSetChannelConfigResult(result);
          } else {
            SUPLA_LOG_WARNING(
                "Error: couldn't find element for a requested channel [%d]",
                result->ChannelNumber);
          }
        }
        break;
      }
      case SUPLA_SD_CALL_SET_CHANNEL_CONFIG: {
        auto *request = rd.data.sds_set_channel_config_request;
        if (request) {
          TSDS_SetChannelConfigResult result = {};
          result.ChannelNumber = request->ChannelNumber;
          result.Result = SUPLA_RESULTCODE_CHANNELNOTFOUND;
          auto element =
              Supla::Element::getElementByChannelNumber(request->ChannelNumber);
          SUPLA_LOG_INFO(
              "Channel[%d] Received SetChannelConfig: fnc %d"
              ", config type %d, config size %d",
              request->ChannelNumber,
              request->Func,
              request->ConfigType,
              request->ConfigSize);

          if (element) {
            switch (request->ConfigType) {
              default:
              case SUPLA_CONFIG_TYPE_DEFAULT: {
                result.Result = element->handleChannelConfig(request);
                break;
              }
              case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE: {
                result.Result = element->handleWeeklySchedule(request);
                break;
              }
              case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE: {
                result.Result = element->handleWeeklySchedule(request, true);
                break;
              }
            }
          } else {
            SUPLA_LOG_WARNING(
                "Error: couldn't find element for a requested channel [%d]",
                request->ChannelNumber);
          }
          SUPLA_LOG_INFO(
              "Channel[%d] Sending SetChannelConfigResult %s (%d)",
              request->ChannelNumber,
              Supla::Protocol::SuplaSrpc::configResultToCStr(result.Result),
              result.Result);
          srpc_ds_async_set_channel_config_result(srpc, &result);
        }
        break;
      }
      case SUPLA_SD_CALL_SET_DEVICE_CONFIG_RESULT: {
        auto *result = rd.data.sds_set_device_config_result;
        if (result) {
          suplaSrpc->handleSetDeviceConfigResult(result);
        }
        break;
      }
      case SUPLA_SD_CALL_SET_DEVICE_CONFIG: {
        auto *request = rd.data.sds_set_device_config_request;
        if (request) {
          suplaSrpc->handleDeviceConfig(request);
        }
        break;
      }
      case SUPLA_SD_CALL_CHANNELGROUP_SET_VALUE: {
        TSD_SuplaChannelGroupNewValue *groupNewValue =
            rd.data.sd_channelgroup_new_value;
        if (groupNewValue) {
          auto element = Supla::Element::getElementByChannelNumber(
              groupNewValue->ChannelNumber);
          if (element) {
            TSD_SuplaChannelNewValue newValue = {};
            newValue.SenderID = 0;
            newValue.ChannelNumber = groupNewValue->ChannelNumber;
            newValue.DurationMS = groupNewValue->DurationMS;
            memcpy(
                newValue.value, groupNewValue->value, SUPLA_CHANNELVALUE_SIZE);
            element->handleNewValueFromServer(&newValue);
          } else {
            SUPLA_LOG_WARNING(
                "Error: couldn't find element for a requested channel [%d]",
                rd.data.sd_channel_new_value->ChannelNumber);
          }
        }
        break;
      }
      case SUPLA_SD_CALL_CHANNEL_CONFIG_FINISHED: {
        auto *request = rd.data.sd_channel_config_finished;
        if (request) {
          auto element =
              Supla::Element::getElementByChannelNumber(request->ChannelNumber);
          SUPLA_LOG_INFO("Channel[%d] Received ChannelConfigFinished",
                         request->ChannelNumber);

          if (element) {
            element->handleChannelConfigFinished();
          } else {
            SUPLA_LOG_WARNING(
                "Error: couldn't find element for a requested channel [%d]",
                request->ChannelNumber);
          }
        }
        break;
      }
      case SUPLA_SCD_CALL_SET_CHANNEL_CAPTION_RESULT:
        SUPLA_LOG_DEBUG("Channel[%d] Receieved setChannelCaptionResult",
                        rd.data.scd_set_caption_result->ChannelNumber);
        break;
      default:
        SUPLA_LOG_WARNING("Received unknown message from server! (call_id: %d)",
                          rd.call_id);
        break;
    }

    srpc_rd_free(&rd);

  } else if (getDataResult == SUPLA_RESULT_DATA_ERROR) {
    SUPLA_LOG_WARNING("DATA ERROR!");
  }
}

void Supla::Protocol::SuplaSrpc::onVersionError(
    TSDC_SuplaVersionError *versionError) {
  (void)(versionError);
  sdc->status(STATUS_PROTOCOL_VERSION_ERROR, F("Protocol version error"));
  SUPLA_LOG_ERROR("Protocol version error. Server min: %d; Server version: %d",
                  versionError->server_version_min,
                  versionError->server_version);

  disconnect();

  lastIterateTime = millis();
  waitForIterate = 15000;
}

void Supla::Protocol::SuplaSrpc::onRegisterResultB(
    TSD_SuplaRegisterDeviceResult_B *registerDeviceResultB) {
  // Handle generic result code
  auto regDevResult =
      reinterpret_cast<TSD_SuplaRegisterDeviceResult *>(registerDeviceResultB);
  onRegisterResult(regDevResult);

  // Handle channel conflict report
  SUPLA_LOG_DEBUG("onRegisterResultB: %d, report size: %d",
                  regDevResult->result_code,
                  registerDeviceResultB->channel_report_size);

  // First, check types of conflicts reported by server
  bool hasConflictInvalidType = false;
  bool hasConflictChannelMissingOnServer = false;
  bool hasConflictChannelMissingOnDevice = false;
  if (registerDeviceResultB->channel_report_size <
      Supla::RegisterDevice::getMaxChannelNumberUsed() + 1) {
    SUPLA_LOG_WARNING(
        "RegisterResultB: conflict server report has %d entries, device has "
        "channel with max number %d",
        registerDeviceResultB->channel_report_size,
        Supla::RegisterDevice::getMaxChannelNumberUsed());
    hasConflictChannelMissingOnServer = true;
  }
  for (int i = 0; i < registerDeviceResultB->channel_report_size; i++) {
    if (registerDeviceResultB->channel_report[i] == 0 &&
        !Supla::RegisterDevice::isChannelNumberFree(i)) {
      SUPLA_LOG_WARNING(
          "Channel[%d]: is present on device, but missing on server side", i);
      hasConflictChannelMissingOnServer = true;
      continue;
    }

    if ((registerDeviceResultB->channel_report[i] &
         CHANNEL_REPORT_CHANNEL_REGISTERED) &&
        Supla::RegisterDevice::isChannelNumberFree(i)) {
      SUPLA_LOG_ERROR(
          "Channel[%d]: is registered on server side, but missing on device",
          i);
      hasConflictChannelMissingOnDevice = true;
      continue;
    }

    if ((registerDeviceResultB->channel_report[i] &
         CHANNEL_REPORT_INCORRECT_CHANNEL_TYPE)) {
      SUPLA_LOG_WARNING("Channel[%d]: channel type mismatch", i);
      hasConflictInvalidType = true;
      continue;
    }
  }
  if (channelConflictResolver) {
    channelConflictResolver->onChannelConflictReport(
        registerDeviceResultB->channel_report,
        registerDeviceResultB->channel_report_size,
        hasConflictInvalidType,
        hasConflictChannelMissingOnServer,
        hasConflictChannelMissingOnDevice);
  }
}

void Supla::Protocol::SuplaSrpc::onRegisterResult(
    TSD_SuplaRegisterDeviceResult *registerDeviceResult) {
  uint32_t serverActivityTimeout = 0;
  setDeviceConfigReceivedAfterRegistration = false;

  if (remoteDeviceConfig != nullptr) {
    SUPLA_LOG_DEBUG("Cleanup remote device config instance");
    delete remoteDeviceConfig;
    remoteDeviceConfig = nullptr;
  }
  Supla::Device::RemoteDeviceConfig::ClearResendAttemptsCounter();

  switch (registerDeviceResult->result_code) {
    // OK scenario
    case SUPLA_RESULTCODE_TRUE:
      serverActivityTimeout = registerDeviceResult->activity_timeout;
      registered = 1;
      SUPLA_LOG_DEBUG(
          "Device registered (activity timeout %d s, server version: %d, "
          "server min version: %d)",
          registerDeviceResult->activity_timeout,
          registerDeviceResult->version,
          registerDeviceResult->version_min);
      lastIterateTime = millis();
      sdc->status(STATUS_REGISTERED_AND_READY, F("Registered and ready"));

      if (serverActivityTimeout != activityTimeoutS) {
        SUPLA_LOG_DEBUG("Changing activity timeout to %d s", activityTimeoutS);
        TDCS_SuplaSetActivityTimeout at;
        at.activity_timeout = activityTimeoutS;
        srpc_dcs_async_set_activity_timeout(srpc, &at);
      }

      for (auto element = Supla::Element::begin(); element != nullptr;
           element = element->next()) {
        element->onRegistered(this);
        delay(0);
      }

      return;

      // NOK scenarios
    case SUPLA_RESULTCODE_TEMPORARILY_UNAVAILABLE:
      sdc->status(
          STATUS_TEMPORARILY_UNAVAILABLE, F("Temporarily unavailable!"), true);
      break;

    case SUPLA_RESULTCODE_GUID_ERROR:
      sdc->status(STATUS_INVALID_GUID, F("Incorrect device GUID!"), true);
      break;

    case SUPLA_RESULTCODE_AUTHKEY_ERROR:
      sdc->status(STATUS_INVALID_AUTHKEY, F("Incorrect AuthKey!"), true);
      break;

    case SUPLA_RESULTCODE_BAD_CREDENTIALS:
      sdc->status(STATUS_BAD_CREDENTIALS,
                  F("Bad credentials - incorrect AuthKey or email"),
                  true);
      break;

    case SUPLA_RESULTCODE_REGISTRATION_DISABLED:
      SUPLA_LOG_INFO("Registration result: registration disabled");
      sdc->status(
          STATUS_REGISTRATION_DISABLED, F("Registration disabled!"), true);
      break;

    case SUPLA_RESULTCODE_DEVICE_LIMITEXCEEDED:
      sdc->status(
          STATUS_DEVICE_LIMIT_EXCEEDED, F("Device limit exceeded!"), true);
      break;

    case SUPLA_RESULTCODE_NO_LOCATION_AVAILABLE:
      sdc->status(
          STATUS_NO_LOCATION_AVAILABLE, F("No location available!"), true);
      break;

    case SUPLA_RESULTCODE_DEVICE_DISABLED:
      sdc->status(STATUS_DEVICE_IS_DISABLED, F("Device is disabled!"), true);
      break;

    case SUPLA_RESULTCODE_LOCATION_DISABLED:
      sdc->status(
          STATUS_LOCATION_IS_DISABLED, F("Location is disabled!"), true);
      break;

    case SUPLA_RESULTCODE_LOCATION_CONFLICT:
      sdc->status(STATUS_LOCATION_CONFLICT, F("Location conflict!"), true);
      break;

    case SUPLA_RESULTCODE_CHANNEL_CONFLICT:
      sdc->status(STATUS_CHANNEL_CONFLICT, F("Channel conflict!"), true);
      break;

    case SUPLA_RESULTCODE_COUNTRY_REJECTED:
      sdc->status(STATUS_COUNTRY_REJECTED, F("Country rejected!"), true);
      break;

    case SUPLA_RESULTCODE_CFG_MODE_REQUESTED:
      SUPLA_LOG_INFO("Registration result: CFG mode requested");
      sdc->requestCfgMode();
      return;

    default:
      sdc->status(STATUS_UNKNOWN_ERROR, F("Unknown registration error"), true);
      SUPLA_LOG_ERROR("Register result code %i",
                      registerDeviceResult->result_code);
      break;
  }

  // server rejected registration
  registered = 2;
}

void Supla::Protocol::SuplaSrpc::onSetActivityTimeoutResult(
    TSDC_SuplaSetActivityTimeoutResult *result) {
  setActivityTimeout(result->activity_timeout);
  SUPLA_LOG_DEBUG("Activity timeout set to %d s", result->activity_timeout);
}

void Supla::Protocol::SuplaSrpc::setActivityTimeout(
    uint32_t activityTimeoutSec) {
  if (activityTimeoutSec < 6) {
    activityTimeoutSec = 6;
  }
  activityTimeoutS = activityTimeoutSec;
}

bool Supla::Protocol::SuplaSrpc::ping() {
  uint32_t _millis = millis();
  // If time from last response is longer than "server_activity_timeout + 10 s",
  // then inform about failure in communication
  if ((_millis - lastResponseMs) / 1000 >=
      (static_cast<uint32_t>(activityTimeoutS) + 10)) {
    return false;
  } else if (_millis - lastPingTimeMs >= 5000 &&
             ((_millis - lastResponseMs) / 1000 >=
                  (static_cast<uint32_t>(activityTimeoutS) - 5) ||
              (_millis - lastSentMs) / 1000 >=
                  (static_cast<uint32_t>(activityTimeoutS) - 5))) {
    lastPingTimeMs = _millis;
    srpc_dcs_async_ping_server(srpc);
  }
  return true;
}

bool Supla::Protocol::SuplaSrpc::iterate(uint32_t _millis) {
  if (!isEnabled()) {
    return false;
  }

  handlePendingCalCfgTimeouts(_millis);

  requestNetworkRestart = false;
  if (waitForIterate != 0 && _millis - lastIterateTime < waitForIterate) {
    return false;
  }

  waitForIterate = 0;

  // Wait for registration (timeout) use lastIterateTime, so we don't change
  // it here if we're waiting for registration reply
  if (registered != -1) {
    lastIterateTime = _millis;
  }

#ifndef ARDUINO_ARCH_AVR
  if (Supla::RegisterDevice::isServerNameEmpty() &&
      !Supla::RegisterDevice::isEmailEmpty()) {
    autodiscoverRetryCounter++;
    if (autodiscoverRetryCounter > 4) {
      if (autodiscoverRetryCounter == 5) {
        SUPLA_LOG_WARNING("Autodiscover failed too many times. Giving up");
      }
      autodiscoverRetryCounter = 6;
      return false;
    }

    // Try to get server from AD
    SUPLA_LOG_INFO("Supla server name not set. Trying to get it from AD");
    // fetch json from https://autodiscover.supla.org/users/email@host
    const char server[] = "iot.autodiscover.supla.org";
    auto adClient = Supla::ClientBuilder();
    adClient->setSSLEnabled(true);
    adClient->setCACert(::suplaCACert);

    if (1 == adClient->connect(server, 443)) {
      adClient->write("GET /users/");
      adClient->write(Supla::RegisterDevice::getEmail());
      adClient->write(" HTTP/1.1\r\n");
      adClient->write("Host: ");
      adClient->write(server);
      adClient->write("\r\n");
#define HTTP_AGENT_SIZE 100
      char httpAgent[HTTP_AGENT_SIZE] = {};
      Supla::RegisterDevice::generateHttpAgent(httpAgent, HTTP_AGENT_SIZE);

      adClient->write("User-Agent: ");
      adClient->write(httpAgent);
      adClient->write("\r\n");
      adClient->write("Accept: application/json\r\n");
      char guid[SUPLA_GUID_SIZE * 2 + 1] = {};
      generateHexString(
          Supla::RegisterDevice::getGUID(), guid, SUPLA_GUID_SIZE);
      adClient->write("X-GUID: ");
      adClient->write(guid);
      adClient->write("\r\n");
      adClient->write("Connection: close\r\n\r\n");

      char buf[512] = {};
      char *bufPos = buf;
      int timeout = 3000;

      bool dataReady = false;
      do {
        int len = adClient->read(bufPos, sizeof(buf) - 1 - (bufPos - buf));
        if (len > 0) {
          SUPLA_LOG_DEBUG("Data read: %d", len);
          bufPos += len;
          *bufPos = 0;
        }
        delay(1);
        timeout--;
        if (timeout == 0) {
          break;
        }
      } while (adClient->connected() && (bufPos - buf) < 512);

      adClient->stop();
      delete adClient;

      SUPLA_LOG_DEBUG("Data: %s", buf);

      // get http return code from string
      if (strncmp(buf, "HTTP/1.1 404", 12) == 0) {
        SUPLA_LOG_DEBUG("HTTP/1.1 404 not found");
        autodiscoverRetryCounter = 6;
        addLastStateAdError(buf);
        return false;
      }

      if (strncmp(buf, "HTTP/1.1 200", 12) != 0) {
        SUPLA_LOG_DEBUG("HTTP/1.1 200 not found");
        addLastStateAdError(buf);
        return false;
      }

      const char serverKey[] = "\"server\":\"";

      char *serverName = strstr(buf, serverKey);
      if (serverName != nullptr) {
        serverName += sizeof(serverKey) - 1;
        char *serverEnd = strchr(serverName, '"');
        if (serverEnd != nullptr) {
          *serverEnd = 0;
          Supla::RegisterDevice::setServerName(serverName);
          dataReady = true;
          char tmp[200] = {};
          snprintf(tmp, sizeof(tmp), "AD got server: %s", serverName);
          sdc->addLastStateLog(tmp);
          auto cfg = Supla::Storage::ConfigInstance();
          if (cfg) {
            cfg->setSuplaServer(serverName);
            cfg->saveWithDelay(1000);
            // reload config to initialize certificates etc.
            onLoadConfig();
          }
        }
      }

      if (!dataReady) {
        SUPLA_LOG_DEBUG("Supla server name not found from AD");
        waitForIterate = 1000;
        return false;
      }

    } else {
      SUPLA_LOG_DEBUG("AD connection failed");
      waitForIterate = 1000;
      return false;
    }

    lastIterateTime = _millis;
  }
#endif  // !ARDUINO_ARCH_AVR

  if (Supla::RegisterDevice::isServerNameEmpty() ||
      Supla::RegisterDevice::isEmailEmpty()) {
    return false;
  }

  if (client == nullptr) {
    initClient();
  }
  // Establish connection with Supla server
  if (!client->connected()) {
    deinitializeSrpc();
    if (registered != 0) {
      SUPLA_LOG_DEBUG("Supla server connection lost. Trying to reconnect");
      sdc->uptime.setConnectionLostCause(
          SUPLA_LASTCONNECTIONRESETCAUSE_SERVER_CONNECTION_LOST);
      registered = 0;
      client->stop();
#ifdef ARDUINO_ARCH_ESP8266
      // Arduino ESP8266 seems to leak memory on SSL reconnect.
      // Workaround: Resetting Wi-Fi cleans it up
      if (isSuplaSSLEnabled) {
        requestNetworkRestart = true;
      }
#endif
      waitForIterate = 1000;
      lastIterateTime = _millis;
      return false;
    }

    if (port == -1) {
      if (isSuplaSSLEnabled) {
        port = 2016;
      } else {
        port = 2015;
      }
    }
    int result = client->connect(Supla::RegisterDevice::getServerName(), port);
    if (1 == result) {
      sdc->uptime.resetConnectionUptime();
      connectionFailCounter = 0;
      //      lastConnectionResetCounter = 0;
      SUPLA_LOG_INFO("Connected to Supla Server");
      initializeSrpc();
    } else {
      if (!firstConnectionAttempt) {
        sdc->status(STATUS_SERVER_DISCONNECTED,
                    F("Not connected to Supla server"));
      }
      SUPLA_LOG_DEBUG("Connection fail (%d). Server: %s",
                      result,
                      Supla::RegisterDevice::getServerName());
      if (firstConnectionAttempt) {
        waitForIterate = 1000;
      } else {
        waitForIterate = 10000;
      }

      disconnect();
      firstConnectionAttempt = false;
      connectionFailCounter++;
      if (connectionFailCounter % 6 == 0) {
        requestNetworkRestart = true;
      }
      return false;
    }
  }

  if (srpc_iterate_device(srpc) == SUPLA_RESULT_FALSE) {
    sdc->status(STATUS_ITERATE_FAIL, F("Communication failure"));
    disconnect();

    lastIterateTime = _millis;
    waitForIterate = 5000;
    return false;
  }

  if (registered == 0) {
    // Perform registration if we are not yet registered
    registered = -1;
    sdc->status(STATUS_REGISTER_IN_PROGRESS, F("Register in progress"));
    if (version <= 24) {
      if (!srpc_ds_async_registerdevice_in_chunks(
              srpc,
              Supla::RegisterDevice::getRegDevHeaderPtr(),
              Supla::RegisterDevice::getChannelPtr_D)) {
        SUPLA_LOG_WARNING("Fatal SRPC failure!");
      }
    } else {
      if (!srpc_ds_async_registerdevice_in_chunks_g(
              srpc,
              Supla::RegisterDevice::getRegDevHeaderPtr(),
              Supla::RegisterDevice::getChannelPtr_E)) {
        SUPLA_LOG_WARNING("Fatal SRPC failure!");
      }
    }
    return false;
  } else if (registered == -1) {
    // Handle registration timeout (in case of no reply received)
    if (_millis - lastIterateTime > 10 * 1000) {
      SUPLA_LOG_DEBUG(
          "No reply to registration message. Resetting connection.");
      sdc->status(STATUS_SERVER_DISCONNECTED,
                  F("Not connected to Supla server"));
      disconnect();

      lastIterateTime = _millis;
      waitForIterate = 2000;
    }
    return false;
  } else if (registered == 1) {
    // Device is registered and everything is correct
    auto cfg = Supla::Storage::ConfigInstance();

    if (setDeviceConfigReceivedAfterRegistration && cfg &&
        cfg->isDeviceConfigChangeReadyToSend() &&
        remoteDeviceConfig == nullptr) {
      SUPLA_LOG_INFO("Sending new device config to server");
      remoteDeviceConfig = new Supla::Device::RemoteDeviceConfig();
      auto *deviceConfig = new TSDS_SetDeviceConfig{};
      if (deviceConfig &&
          remoteDeviceConfig->fillSetDeviceConfig(deviceConfig)) {
        srpc_ds_async_set_device_config_request(srpc, deviceConfig);
        delete deviceConfig;
      } else {
        delete deviceConfig;
        cfg->clearDeviceConfigChangeFlag();
        cfg->saveWithDelay(1000);
      }
    }

    if (ping() == false) {
      sdc->uptime.setConnectionLostCause(
          SUPLA_LASTCONNECTIONRESETCAUSE_ACTIVITY_TIMEOUT);
      SUPLA_LOG_DEBUG("TIMEOUT - lost connection with server");
      sdc->status(STATUS_SERVER_DISCONNECTED,
                  F("Not connected to Supla server"));
      disconnect();
    }

    return true;
  } else if (registered == 2) {
    // Server rejected registration
    disconnect();
    registered = 0;
    lastIterateTime = millis();
    waitForIterate = 10000;
  }
  return false;
}

void Supla::Protocol::SuplaSrpc::addLastStateAdError(char *buf) {
  if (adErrorLogged) {
    return;
  }

  const char errorKey[] = "\"error\":\"";

  auto error = strstr(buf, errorKey);
  if (error != nullptr) {
    error += sizeof(errorKey) - 1;
    auto errorEnd = strchr(error, '"');
    if (errorEnd != nullptr) {
      *errorEnd = 0;
      char tmp[200] = {};
      snprintf(tmp, sizeof(tmp), "AD error: %s", error);
      sdc->addLastStateLog(tmp);
      adErrorLogged = true;
    } else {
      sdc->addLastStateLog("AD error: unknown");
      adErrorLogged = true;
    }
  }
}

void Supla::Protocol::SuplaSrpc::disconnect() {
  if (!isEnabled()) {
    return;
  }

  firstConnectionAttempt = true;
  registered = 0;
  if (client) {
    client->stop();
  }
  deinitializeSrpc();
}

void Supla::Protocol::SuplaSrpc::updateLastResponseTime() {
  lastResponseMs = millis();
}

void Supla::Protocol::SuplaSrpc::updateLastSentTime() {
  lastSentMs = millis();
}

void Supla::Protocol::SuplaSrpc::setServerPort(int value) {
  port = value;
}

void Supla::Protocol::SuplaSrpc::setVersion(int value) {
  version = value;
}

void Supla::Protocol::SuplaSrpc::setSuplaCACert(const char *cert) {
  suplaCACert = cert;
}

void Supla::Protocol::SuplaSrpc::setSupla3rdPartyCACert(const char *cert) {
  supla3rdPartyCACert = cert;
}

const char *Supla::Protocol::SuplaSrpc::getSuplaCACert() const {
  return suplaCACert;
}

const char *Supla::Protocol::SuplaSrpc::getSupla3rdPartyCACert() const {
  return supla3rdPartyCACert;
}

void Supla::Protocol::SuplaSrpc::onGetUserLocaltimeResult(
    TSDC_UserLocalTimeResult *result) {
  auto clock = getSdc()->getClock();
  if (clock) {
    clock->parseLocaltimeFromServer(result);
  }
}

bool Supla::Protocol::SuplaSrpc::isNetworkRestartRequested() {
  return requestNetworkRestart;
}

uint32_t Supla::Protocol::SuplaSrpc::getConnectionFailTime() {
  // connectionFailCounter is incremented every 10 s
  return connectionFailCounter * 10;
}

bool Supla::Protocol::SuplaSrpc::verifyConfig() {
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg && !cfg->isSuplaCommProtocolEnabled()) {
    // skip verification of Supla protocol if it is disabled
    return true;
  }

  if (Supla::RegisterDevice::isServerNameEmpty()) {
    sdc->status(STATUS_UNKNOWN_SERVER_ADDRESS, F("Missing server address"));
#ifdef ARDUINO_ARCH_AVR
    return false;
#endif  // ARDUINO_ARCH_AVR
  }

  if (Supla::RegisterDevice::isEmailEmpty()) {
    sdc->status(STATUS_MISSING_CREDENTIALS, F("Missing email address"));
    if (sdc->getDeviceMode() != Supla::DEVICE_MODE_CONFIG) {
      return false;
    }
  }

  return true;
}

// By default Supla protocol is enabled.
// It can be disabled if we have config class instance and when it is
// disabled there.
bool Supla::Protocol::SuplaSrpc::isEnabled() {
  return enabled;
}

void Supla::Protocol::SuplaSrpc::sendChannelStateResult(int32_t receiverId,
                                                        uint8_t channelNo) {
  TDSC_ChannelState state = {};
  state.ReceiverID = receiverId;
  state.ChannelNumber = channelNo;
  if (client->getSrcConnectionIPAddress() != 0) {
    state.Fields |= SUPLA_CHANNELSTATE_FIELD_IPV4;
    state.IPv4 = client->getSrcConnectionIPAddress();
  }

  auto network =
      Supla::Network::GetInstanceByIP(client->getSrcConnectionIPAddress());
  if (network) {
    network->fillStateData(&state);
  } else {
    Supla::Network::Instance()->fillStateData(&state);
  }
  getSdc()->fillStateData(&state);
  auto element = Supla::Element::getElementByChannelNumber(channelNo);
  if (element) {
    element->handleGetChannelState(&state);
  }
  srpc_csd_async_channel_state_result(srpc, &state);
}

uint32_t Supla::Protocol::SuplaSrpc::getActivityTimeout() {
  return activityTimeoutS;
}

bool Supla::Protocol::SuplaSrpc::isUpdatePending() {
  if (sdc->isRemoteDeviceConfigEnabled()) {
    if (!setDeviceConfigReceivedAfterRegistration) {
      return true;
    }
  }
  return Supla::Element::IsAnyUpdatePending();
}

bool Supla::Protocol::SuplaSrpc::isRegisteredAndReady() {
  return registered == 1;
}

void Supla::Protocol::SuplaSrpc::sendActionTrigger(uint8_t channelNumber,
                                                   uint32_t actionId) {
  if (!isRegisteredAndReady()) {
    return;
  }

  TDS_ActionTrigger at = {};
  at.ChannelNumber = channelNumber;
  at.ActionTrigger = actionId;
  srpc_ds_async_action_trigger(srpc, &at);
}

bool Supla::Protocol::SuplaSrpc::sendNotification(int context,
                                                  const char *title,
                                                  const char *message,
                                                  int soundId) {
  if (!isRegisteredAndReady()) {
    return false;
  }

  TDS_PushNotification notif = {};
  notif.Context = context;
  if (soundId >= 0) {
    notif.SoundId = soundId;
  }
  int titleLen = (title == nullptr ? 0 : strlen(title));
  int messageLen = (message == nullptr ? 0 : strlen(message));

  notif.TitleSize = (titleLen == 0 ? 0 : titleLen + 1);
  notif.BodySize = (messageLen == 0 ? 0 : messageLen + 1);

  if (titleLen >= SUPLA_PN_TITLE_MAXSIZE) {
    titleLen = SUPLA_PN_TITLE_MAXSIZE - 1;
  }
  if (titleLen > 0) {
    memcpy(notif.TitleAndBody, title, titleLen);
    notif.TitleAndBody[titleLen] = '\0';
  }
  if (messageLen >= SUPLA_PN_BODY_MAXSIZE) {
    messageLen = SUPLA_PN_BODY_MAXSIZE - 1;
  }
  int titleOffset = (titleLen == 0 ? 0 : titleLen + 1);
  if (messageLen > 0) {
    memcpy(notif.TitleAndBody + titleOffset, message, messageLen + 1);
    notif.TitleAndBody[titleOffset + messageLen] = '\0';
  }
  srpc_ds_async_send_push_notification(srpc, &notif);
  return true;
}

void Supla::Protocol::SuplaSrpc::getUserLocaltime() {
  if (!isRegisteredAndReady()) {
    return;
  }
  srpc_dcs_async_get_user_localtime(srpc);
}

void Supla::Protocol::SuplaSrpc::sendChannelValueChanged(
    uint8_t channelNumber,
    int8_t *value,
    uint8_t offline,
    uint32_t validityTimeSec) {
  if (!isRegisteredAndReady()) {
    return;
  }
  srpc_ds_async_channel_value_changed_c(srpc,
                                        channelNumber,
                                        reinterpret_cast<char *>(value),
                                        offline,
                                        validityTimeSec);
}

void Supla::Protocol::SuplaSrpc::sendExtendedChannelValueChanged(
    uint8_t channelNumber, TSuplaChannelExtendedValue *value) {
  if (!isRegisteredAndReady()) {
    return;
  }
  srpc_ds_async_channel_extendedvalue_changed(srpc, channelNumber, value);
}

void Supla::Protocol::SuplaSrpc::getChannelConfig(uint8_t channelNumber,
                                                  uint8_t configType) {
  if (!isRegisteredAndReady()) {
    return;
  }
  TDS_GetChannelConfigRequest request = {};
  request.ChannelNumber = channelNumber;
  request.ConfigType = configType;
  srpc_ds_async_get_channel_config_request(srpc, &request);
}

bool Supla::Protocol::SuplaSrpc::setChannelConfig(uint8_t channelNumber,
                                                  _supla_int_t channelFunction,
                                                  void *channelConfig,
                                                  int size,
                                                  uint8_t configType) {
  if (!isRegisteredAndReady()) {
    return false;
  }
  if ((size != 0 && channelConfig == nullptr) ||
      (size == 0 && channelConfig != nullptr) || (size < 0) ||
      (size > SUPLA_CHANNEL_CONFIG_MAXSIZE)) {
    return false;
  }

  TSDS_SetChannelConfig request = {};
  request.ChannelNumber = channelNumber;
  request.Func = channelFunction;
  request.ConfigType = configType;
  request.ConfigSize = size;
  memcpy(request.Config, channelConfig, size);
  srpc_ds_async_set_channel_config_request(srpc, &request);
  return true;
}

bool Supla::Protocol::SuplaSrpc::setDeviceConfig(
    TSDS_SetDeviceConfig *deviceConfig) {
  if (!isRegisteredAndReady()) {
    return false;
  }
  if (deviceConfig == nullptr) {
    return false;
  }

  deviceConfig->EndOfDataFlag = 1;
  srpc_ds_async_set_device_config_request(srpc, deviceConfig);
  return true;
}

bool Supla::Protocol::SuplaSrpc::setInitialCaption(uint8_t channelNumber,
                                                   const char *caption) {
  if (!isRegisteredAndReady()) {
    return false;
  }
  TDCS_SetCaption *request = new TDCS_SetCaption;
  if (request == nullptr) {
    return false;
  }
  request->ChannelNumber = channelNumber;
  strncpy(request->Caption, caption, SUPLA_CAPTION_MAXSIZE);
  request->Caption[SUPLA_CAPTION_MAXSIZE - 1] = '\0';
  request->CaptionSize = strnlen(request->Caption, SUPLA_CAPTION_MAXSIZE) + 1;
  srpc_dcs_async_set_channel_caption(srpc, request);
  delete request;
  return true;
}

void Supla::Protocol::SuplaSrpc::handleDeviceConfig(
    TSDS_SetDeviceConfig *request) {
  SUPLA_LOG_INFO("Received new device config");
  if (remoteDeviceConfig == nullptr) {
    remoteDeviceConfig = new Supla::Device::RemoteDeviceConfig(
        !setDeviceConfigReceivedAfterRegistration);
  }

  remoteDeviceConfig->processConfig(request);

  if (remoteDeviceConfig->isEndFlagReceived()) {
    {
      TSDS_SetDeviceConfigResult result = {};
      result.Result = remoteDeviceConfig->getResultCode();
      SUPLA_LOG_INFO("Sending device config result %s (%d)",
                     configResultToCStr(result.Result),
                     result.Result);
      srpc_ds_async_set_device_config_result(srpc, &result);
    }

    if (remoteDeviceConfig->isSetDeviceConfigRequired()) {
      auto *deviceConfig = new TSDS_SetDeviceConfig{};
      if (deviceConfig &&
          remoteDeviceConfig->fillSetDeviceConfig(deviceConfig)) {
        srpc_ds_async_set_device_config_request(srpc, deviceConfig);
        delete deviceConfig;
      } else {
        delete deviceConfig;
        auto cfg = Supla::Storage::ConfigInstance();
        if (cfg) {
          cfg->clearDeviceConfigChangeFlag();
          cfg->saveWithDelay(1000);
        }
      }
    } else {
      // procedure ends here, so delete the handler
      setDeviceConfigReceivedAfterRegistration = true;
      delete remoteDeviceConfig;
      remoteDeviceConfig = nullptr;
    }
  }
}

void Supla::Protocol::SuplaSrpc::handleSetDeviceConfigResult(
    TSDS_SetDeviceConfigResult *result) {
  if (result == nullptr) {
    return;
  }

  SUPLA_LOG_INFO("Received set device config result %s (%d)",
                 configResultToCStr(result->Result),
                 result->Result);

  if (remoteDeviceConfig == nullptr) {
    SUPLA_LOG_WARNING("Unexpected set device config result - missing handler");
    return;
  }

  remoteDeviceConfig->handleSetDeviceConfigResult(result);
  setDeviceConfigReceivedAfterRegistration = true;
  delete remoteDeviceConfig;
  remoteDeviceConfig = nullptr;
}

const char *Supla::Protocol::SuplaSrpc::configResultToCStr(int result) {
  switch (result) {
    case SUPLA_CONFIG_RESULT_TRUE:
      return "OK";
    case SUPLA_CONFIG_RESULT_FALSE:
      return "FALSE (NOK)";
    case SUPLA_CONFIG_RESULT_DATA_ERROR:
      return "DATA_ERROR";
    case SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED:
      return "TYPE_NOT_SUPPORTED";
    case SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED:
      return "FUNCTION_NOT_SUPPORTED";
    case SUPLA_CONFIG_RESULT_LOCAL_CONFIG_DISABLED:
      return "LOCAL_CONFIG_DISABLED";
    default:
      return "UNKNOWN";
  }
}

void Supla::Protocol::SuplaSrpc::sendRegisterNotification(
    TDS_RegisterPushNotification *notification) {
  if (!isRegisteredAndReady() || notification == nullptr) {
    return;
  }
  int result = srpc_ds_async_register_push_notification(srpc, notification);
  if (result == 0) {
    SUPLA_LOG_WARNING("Sending register notification failed");
  }
}

void Supla::Protocol::SuplaSrpc::sendSubdeviceDetails(
    TDS_SubdeviceDetails *subdeviceDetails) {
  if (!isRegisteredAndReady() || subdeviceDetails == nullptr) {
    return;
  }
  int result = srpc_ds_async_set_subdevice_details(srpc, subdeviceDetails);
  if (result == 0) {
    SUPLA_LOG_WARNING("Sending subdevice details failed");
  }
}

void Supla::Protocol::SuplaSrpc::sendRemainingTimeValue(uint8_t channelNumber,
                                                        uint32_t timeMs,
                                                        uint8_t state,
                                                        int32_t senderId) {
  if (!isRegisteredAndReady()) {
    SUPLA_LOG_DEBUG("SuplaSrpc:: timer update not registered");
    return;
  }
  auto *value = new TSuplaChannelExtendedValue;
  memset(value, 0, sizeof(TSuplaChannelExtendedValue));
  value->type = EV_TYPE_TIMER_STATE_V1;
  value->size = sizeof(TTimerState_ExtendedValue);

  TTimerState_ExtendedValue *timerState =
      reinterpret_cast<TTimerState_ExtendedValue *>(&value->value);

  timerState->SenderID = senderId;
  timerState->TargetValue[0] = state;
  timerState->RemainingTimeMs = timeMs;

  SUPLA_LOG_DEBUG("SRPC sending: remaining time %d, channel %d, state %d",
                  timeMs,
                  channelNumber,
                  state);
  srpc_ds_async_channel_extendedvalue_changed(srpc, channelNumber, value);
  delete value;
}

void Supla::Protocol::SuplaSrpc::sendRemainingTimeValue(
    uint8_t channelNumber,
    uint32_t remainingTime,
    unsigned char state[SUPLA_CHANNELVALUE_SIZE],
    int32_t senderId,
    bool useSecondsInsteadOfMs) {
  if (!isRegisteredAndReady()) {
    SUPLA_LOG_DEBUG("SuplaSrpc:: timer update not send - not registered");
    return;
  }
  auto *value = new TSuplaChannelExtendedValue;
  memset(value, 0, sizeof(TSuplaChannelExtendedValue));
  if (useSecondsInsteadOfMs) {
    value->type = EV_TYPE_TIMER_STATE_V1_SEC;
  } else {
    value->type = EV_TYPE_TIMER_STATE_V1;
  }
  value->size = sizeof(TTimerState_ExtendedValue);

  TTimerState_ExtendedValue *timerState =
      reinterpret_cast<TTimerState_ExtendedValue *>(&value->value);

  timerState->SenderID = senderId;
  memcpy(timerState->TargetValue, state, sizeof(timerState->TargetValue));
  if (useSecondsInsteadOfMs) {
    timerState->RemainingTimeMs = remainingTime;
  } else {
    timerState->RemainingTimeS = remainingTime;
  }

  SUPLA_LOG_DEBUG("SRPC sending: remaining time %d %s, channel %d",
                  remainingTime,
                  useSecondsInsteadOfMs ? "s" : "ms",
                  channelNumber);
  srpc_ds_async_channel_extendedvalue_changed(srpc, channelNumber, value);
  delete value;
}

void Supla::Protocol::CalCfgResultPending::set(int16_t channelNo,
                                               int32_t receiverId,
                                               int32_t command,
                                               uint32_t timeoutMs) {
  auto ptr = first;
  CalCfgResultPendingItem *prev = nullptr;
  uint32_t now = millis();
  if (now == 0) {
    now = 1;
  }
  while (ptr) {
    if (ptr->channelNo == channelNo &&
        (command == -1 || ptr->command == command)) {
      ptr->receiverId = receiverId;
      ptr->command = command;
      ptr->timeoutMs = timeoutMs;
      ptr->createdAtMs = now;
      return;
    }
    prev = ptr;
    ptr = ptr->next;
  }
  auto item = new CalCfgResultPendingItem;
  item->channelNo = channelNo;
  item->receiverId = receiverId;
  item->command = command;
  item->createdAtMs = now;
  item->timeoutMs = timeoutMs;
  item->next = nullptr;

  if (first == nullptr) {
    first = item;
  } else if (prev) {
    prev->next = item;
  }
}

void Supla::Protocol::CalCfgResultPending::clear(int16_t channelNo,
                                                 int32_t command) {
  auto ptr = first;
  CalCfgResultPendingItem *prev = nullptr;
  while (ptr) {
    if (ptr->channelNo == channelNo &&
        (command == -1 || ptr->command == command)) {
      if (prev) {
        prev->next = ptr->next;
      } else {
        first = ptr->next;
      }
      delete ptr;
      return;
    }
    prev = ptr;
    ptr = ptr->next;
  }
}

void Supla::Protocol::CalCfgResultPending::clearTimeout(int16_t channelNo,
                                                        int32_t command) {
  auto ptr = first;
  while (ptr) {
    if (ptr->channelNo == channelNo &&
        (command == -1 || ptr->command == command)) {
      ptr->timeoutMs = 0;
      return;
    }
    ptr = ptr->next;
  }
}

void Supla::Protocol::CalCfgResultPending::clearAll() {
  while (first) {
    auto copy = first;
    first = first->next;
    delete copy;
  }
}

Supla::Protocol::CalCfgResultPending::CalCfgResultPending() {
}

Supla::Protocol::CalCfgResultPending::~CalCfgResultPending() {
  clearAll();
}

Supla::Protocol::CalCfgResultPendingItem *
Supla::Protocol::CalCfgResultPending::get(int16_t channelNo, int32_t command) {
  auto ptr = first;
  while (ptr) {
    if (ptr->channelNo == channelNo &&
        (command == -1 || ptr->command == command)) {
      return ptr;
    }
    ptr = ptr->next;
  }
  return nullptr;
}

void Supla::Protocol::SuplaSrpc::sendPendingCalCfgResult(int16_t channelNo,
                                                         int32_t resultId,
                                                         int32_t command,
                                                         int dataSize,
                                                         void *data) {
  sendPendingCalCfgResultForCommand(
      channelNo, resultId, command, command, dataSize, data);
}

void Supla::Protocol::SuplaSrpc::sendPendingCalCfgResultForCommand(
    int16_t channelNo,
    int32_t resultId,
    int32_t pendingCommand,
    int32_t responseCommand,
    int dataSize,
    void *data) {
  if (srpc == nullptr) {
    SUPLA_LOG_WARNING("No active SRPC for CALCFG response on channel %d",
                      channelNo);
    return;
  }
  auto pendingResponse = calCfgResultPending.get(channelNo, pendingCommand);
  if (pendingResponse == nullptr) {
    SUPLA_LOG_WARNING("No pending response for channel %d", channelNo);
    return;
  }

  TDS_DeviceCalCfgResult result = {};
  result.ReceiverID = pendingResponse->receiverId;
  result.ChannelNumber = channelNo;
  result.Command = pendingResponse->command;
  if (responseCommand >= 0) {
    result.Command = responseCommand;
  }

  result.Result = resultId;
  if (dataSize > SUPLA_CALCFG_DATA_MAXSIZE) {
    SUPLA_LOG_WARNING("Data size %d is too big", dataSize);
    dataSize = SUPLA_CALCFG_DATA_MAXSIZE;
  }
  result.DataSize = dataSize;
  if (dataSize > 0 && data != nullptr) {
    memcpy(result.Data, data, dataSize);
  }
  SUPLA_LOG_DEBUG("Sending CALCFG result: cmd %d (0x%X) result: %d",
                  result.Command,
                  result.Command,
                  result.Result);
  srpc_ds_async_device_calcfg_result(srpc, &result);
}

void Supla::Protocol::SuplaSrpc::sendCalCfgResult(int32_t receiverId,
                                                  int16_t channelNo,
                                                  int32_t resultId,
                                                  int32_t command,
                                                  int dataSize,
                                                  void *data) {
  if (srpc == nullptr) {
    SUPLA_LOG_WARNING("No active SRPC for CALCFG response on channel %d",
                      channelNo);
    return;
  }

  TDS_DeviceCalCfgResult result = {};
  result.ReceiverID = receiverId;
  result.ChannelNumber = channelNo;
  result.Command = command;
  result.Result = resultId;
  if (dataSize > SUPLA_CALCFG_DATA_MAXSIZE) {
    SUPLA_LOG_WARNING("Data size %d is too big", dataSize);
    dataSize = SUPLA_CALCFG_DATA_MAXSIZE;
  }
  result.DataSize = dataSize;
  if (dataSize > 0 && data != nullptr) {
    memcpy(result.Data, data, dataSize);
  }
  SUPLA_LOG_DEBUG("Sending CALCFG result: cmd %d (0x%X) result: %d",
                  result.Command,
                  result.Command,
                  result.Result);
  srpc_ds_async_device_calcfg_result(srpc, &result);
}

void Supla::Protocol::SuplaSrpc::clearPendingCalCfgResult(int16_t channelNo,
                                                          int32_t command) {
  calCfgResultPending.clear(channelNo, command);
}

void Supla::Protocol::SuplaSrpc::clearPendingCalCfgTimeout(int16_t channelNo,
                                                           int32_t command) {
  calCfgResultPending.clearTimeout(channelNo, command);
}

void Supla::Protocol::SuplaSrpc::handlePendingCalCfgTimeouts(uint32_t _millis) {
  auto ptr = calCfgResultPending.first;
  while (ptr) {
    auto next = ptr->next;
    if (ptr->timeoutMs > 0 &&
        static_cast<uint32_t>(_millis - ptr->createdAtMs) >= ptr->timeoutMs) {
      SUPLA_LOG_WARNING("CALCFG timeout for channel %d, cmd %d (0x%X)",
                        ptr->channelNo,
                        ptr->command,
                        ptr->command);
      sendPendingCalCfgResult(
          ptr->channelNo, SUPLA_CALCFG_RESULT_FALSE, ptr->command);
      calCfgResultPending.clear(ptr->channelNo, ptr->command);
    }
    ptr = next;
  }
}

void Supla::Protocol::SuplaSrpc::initializeSrpc() {
  if (srpc) {
    deinitializeSrpc();
  }

  SUPLA_LOG_INFO("Initializing SRPC (proto: %d)", version);
  TsrpcParams srpcParams;
  srpc_params_init(&srpcParams);
  srpcParams.data_read = &Supla::dataRead;
  srpcParams.data_write = &Supla::dataWrite;
  srpcParams.on_remote_call_received = &Supla::messageReceived;
#ifdef SRPC_WITH_PACKET_LOG_HOOKS
  srpcParams.on_packet_sent = &Supla::Protocol::SuplaSrpc::onPacketSent;
  srpcParams.on_packet_received = &Supla::Protocol::SuplaSrpc::onPacketReceived;
#endif
  srpcParams.user_params = this;

  srpc = srpc_init(&srpcParams);

  // Set Supla protocol interface version
  srpc_set_proto_version(srpc, version);
}

void Supla::Protocol::SuplaSrpc::deinitializeSrpc() {
  calCfgResultPending.clearAll();
  if (srpc) {
    SUPLA_LOG_INFO("Deinitializing SRPC");
    srpc_free(srpc);
    srpc = nullptr;
  }
  setDeviceConfigReceivedAfterRegistration = false;
}

void Supla::Protocol::SuplaSrpc::setChannelConflictResolver(
    Supla::Device::ChannelConflictResolver *resolver) {
  channelConflictResolver = resolver;
}
