/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <SuplaDevice.h>
#include <supla-common/srpc.h>
#include <supla/log_wrapper.h>
#include <supla/network/network.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <supla/network/client.h>
#include <supla/device/remote_device_config.h>
#include <supla/device/register_device.h>
#include <supla/device/channel_conflict_resolver.h>

#include <string.h>

#include "supla_srpc.h"

#define SUPLA_EXT_CHANNEL_STATE_INTERVAL_MS (20 * 60 * 1000)  // 20 min

namespace Supla::Protocol {
struct CalCfgResultPendingItem {
  int16_t channelNo = 0;
  int32_t receiverId = 0;
  int32_t command = 0;

  CalCfgResultPendingItem *next = nullptr;
};
}  // namespace Supla::Protocol

bool Supla::Protocol::SuplaSrpc::isSuplaSSLEnabled = true;

static const char wrongCert[] = "SUPLA";

Supla::Protocol::SuplaSrpc::SuplaSrpc(SuplaDeviceClass *sdc, int version)
    : Supla::Protocol::ProtocolLayer(sdc), version(version) {
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
      delete [] selectedCertificate;
    }
  }
}

void Supla::Protocol::SuplaSrpc::setNetworkClient(Supla::Client *newClient) {
  bool debugLogs = verboseLog;
  if (client) {
    debugLogs = client->isDebugLogs();
    delete client;
  }
  client = newClient;
  client->setDebugLogs(debugLogs);
  initClient();
}

void Supla::Protocol::SuplaSrpc::initClient() {
  if (client == nullptr) {
    if (Supla::Network::Instance()) {
      client = Supla::Network::Instance()->createClient();
    } else {
      client = Supla::ClientBuilder();
    }
    client->setDebugLogs(verboseLog);
  }
  client->setSdc(sdc);
  if (port == 2016 || (port == -1 && isSuplaSSLEnabled)) {
    client->setSSLEnabled(true);
    client->setCACert(selectedCertificate);
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
      configComplete = false;
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
          securityLevel == 2 ? "Skip CA check" :
          securityLevel == 1 ? "Custom CA" :
          "Supla CA");
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
  auto srpcLayer = reinterpret_cast<Supla::Protocol::SuplaSrpc*>(userParams);
  return srpcLayer->client->read(reinterpret_cast<uint8_t*>(buf), count);
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
            "CALCFG CMD received: senderId %d, ch %d, cmd %d, suauth %d, "
            "datatype %d, datasize %d",
            rd.data.sd_device_calcfg_request->SenderID,
            rd.data.sd_device_calcfg_request->ChannelNumber,
            rd.data.sd_device_calcfg_request->Command,
            rd.data.sd_device_calcfg_request->SuperUserAuthorized,
            rd.data.sd_device_calcfg_request->DataType,
            rd.data.sd_device_calcfg_request->DataSize);

        if (rd.data.sd_device_calcfg_request->ChannelNumber == -1) {
          // calcfg with channel == -1 are for whole device, so we route
          // it to SuplaDeviceClass instance
          result.Result = suplaSrpc->getSdc()->handleCalcfgFromServer(
              rd.data.sd_device_calcfg_request, &result);
          if (result.Command == SUPLA_CALCFG_CMD_START_SUBDEVICE_PAIRING &&
              result.Result == SUPLA_CALCFG_RESULT_TRUE) {
            suplaSrpc->calCfgResultPending.set(
                result.ChannelNumber, result.ReceiverID, result.Command);
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
                  result.ChannelNumber, result.ReceiverID, result.Command);
            } else if (result.Result == SUPLA_CALCFG_RESULT_NOT_SUPPORTED) {
              // if request wasn't processed by channel, try to check if there
              // is element related to it's subdevice (if any)
              SUPLA_LOG_DEBUG("Trying to find subdevice for channel %d",
                              rd.data.sd_device_calcfg_request->ChannelNumber);
              auto subdevice = element->getChannel()->getSubDeviceId();
              if (subdevice > 0) {
                auto subdeviceElement =
                    Supla::Element::getOwnerOfSubDeviceId(subdevice);
                if (subdeviceElement) {
                  result.Result = subdeviceElement->handleCalcfgFromServer(
                      rd.data.sd_device_calcfg_request);
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
          SUPLA_LOG_DEBUG("Sending CALCFG result: CMD %d result: %d",
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
          auto element = Supla::Element::getElementByChannelNumber(
              request->ChannelNumber);
          SUPLA_LOG_INFO("Received SetChannelConfig for channel %d, function %d"
              ", config type %d, config size %d", request->ChannelNumber,
              request->Func, request->ConfigType, request->ConfigSize);

          if (element) {
            switch (request->ConfigType) {
              default:
              case SUPLA_CONFIG_TYPE_DEFAULT: {
                result.Result =
                    element->handleChannelConfig(request);
                break;
              }
              case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE: {
                result.Result =
                    element->handleWeeklySchedule(request);
                break;
              }
              case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE: {
                result.Result =
                    element->handleWeeklySchedule(request, true);
                break;
              }
            }
          } else {
            SUPLA_LOG_WARNING(
                "Error: couldn't find element for a requested channel [%d]",
                request->ChannelNumber);
          }
          SUPLA_LOG_INFO("Sending channel config result %s (%d)",
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
          auto element = Supla::Element::getElementByChannelNumber(
              request->ChannelNumber);
          SUPLA_LOG_INFO("Received ChannelConfigFinished for channel [%d]",
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
        SUPLA_LOG_DEBUG("Receieved setChannelCaptionResult for channel [%d]",
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
  auto regDevResult = reinterpret_cast<TSD_SuplaRegisterDeviceResult*>(
      registerDeviceResultB);
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
      Supla::RegisterDevice::getMaxChannelNumberUsed()) {
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
        hasConflictInvalidType, hasConflictChannelMissingOnServer,
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

      // schedule next extended channel state update in 5s
      lastExtendedValueWithChannelStateMs =
          millis() - SUPLA_EXT_CHANNEL_STATE_INTERVAL_MS + 5000;

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
      sdc->requestCfgMode(Supla::Device::WithTimeout);
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
      TSDS_SetDeviceConfig deviceConfig = {};
      if (remoteDeviceConfig->fillSetDeviceConfig(&deviceConfig)) {
        srpc_ds_async_set_device_config_request(srpc, &deviceConfig);
      } else {
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

    if (millis() - lastExtendedValueWithChannelStateMs >
        SUPLA_EXT_CHANNEL_STATE_INTERVAL_MS) {
      for (auto el= Supla::Element::begin(); el!= nullptr; el = el->next()) {
        if (el->getChannel() != nullptr && el->isChannelStateEnabled() &&
            el->getChannel()->isOnline() &&
            el->getChannel()->isBatteryPoweredFieldEnabled()) {
          SUPLA_LOG_DEBUG("SRPC: Sending extended channel state for %d",
                          el->getChannelNumber());
          sendChannelStateResult(0, el->getChannelNumber(), true);
        }
        delay(0);
      }

      lastExtendedValueWithChannelStateMs = millis();
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

const char* Supla::Protocol::SuplaSrpc::getSuplaCACert() {
  return suplaCACert;
}

const char* Supla::Protocol::SuplaSrpc::getSupla3rdPartyCACert() {
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
    if (sdc->getDeviceMode() != Supla::DEVICE_MODE_CONFIG) {
      return false;
    }
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
                                                        uint8_t channelNo,
                                                        bool sendAsExtended) {
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

  TSuplaChannelExtendedValue extValue = {};
  extValue.type = EV_TYPE_CHANNEL_STATE_V1;
  extValue.size = sizeof(state);
  memcpy(extValue.value, &state, sizeof(state));
  sendExtendedChannelValueChanged(channelNo, &extValue);
  // TODO(klew): remove below lines and keep only sending channel state
  // as extended value
  if (!sendAsExtended) {
    srpc_csd_async_channel_state_result(srpc, &state);
  }
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

void Supla::Protocol::SuplaSrpc::sendActionTrigger(
    uint8_t channelNumber, uint32_t actionId) {
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
    uint8_t channelNumber,
    TSuplaChannelExtendedValue *value) {
  if (!isRegisteredAndReady()) {
    return;
  }
  srpc_ds_async_channel_extendedvalue_changed(srpc, channelNumber,
      value);
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
    _supla_int_t channelFunction, void *channelConfig, int size,
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
  TDCS_SetCaption request = {};
  request.ChannelNumber = channelNumber;
  strncpy(request.Caption, caption, SUPLA_CAPTION_MAXSIZE);
  request.Caption[SUPLA_CAPTION_MAXSIZE - 1] = '\0';
  request.CaptionSize = strnlen(request.Caption, SUPLA_CAPTION_MAXSIZE) + 1;
  srpc_dcs_async_set_channel_caption(srpc, &request);
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
                     configResultToCStr(result.Result), result.Result);
      srpc_ds_async_set_device_config_result(srpc, &result);
    }

    if (remoteDeviceConfig->isSetDeviceConfigRequired()) {
      TSDS_SetDeviceConfig deviceConfig = {};
      if (remoteDeviceConfig->fillSetDeviceConfig(&deviceConfig)) {
        srpc_ds_async_set_device_config_request(srpc, &deviceConfig);
      } else {
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
                  timeMs, channelNumber, state);
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
                                               int32_t command) {
  auto ptr = first;
  CalCfgResultPendingItem *prev = nullptr;
  while (ptr) {
    if (ptr->channelNo == channelNo) {
      ptr->receiverId = receiverId;
      ptr->command = command;
      return;
    }
    prev = ptr;
    ptr = ptr->next;
  }
  auto item = new CalCfgResultPendingItem;
  item->channelNo = channelNo;
  item->receiverId = receiverId;
  item->command = command;
  item->next = nullptr;

  if (first == nullptr) {
    first = item;
  } else if (prev) {
    prev->next = item;
  }
}

void Supla::Protocol::CalCfgResultPending::clear(int16_t channelNo) {
  auto ptr = first;
  CalCfgResultPendingItem *prev = nullptr;
  while (ptr) {
    if (ptr->channelNo == channelNo) {
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

Supla::Protocol::CalCfgResultPending::CalCfgResultPending() {
}

Supla::Protocol::CalCfgResultPending::~CalCfgResultPending() {
  while (first) {
    auto copy = first;
    first = first->next;
    delete copy;
  }
}

Supla::Protocol::CalCfgResultPendingItem *
Supla::Protocol::CalCfgResultPending::get(int16_t channelNo) {
  auto ptr = first;
  while (ptr) {
    if (ptr->channelNo == channelNo) {
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
  auto pendingResponse = calCfgResultPending.get(channelNo);
  if (pendingResponse == nullptr) {
    SUPLA_LOG_WARNING("No pending response for channel %d", channelNo);
    return;
  }

  TDS_DeviceCalCfgResult result = {};
  result.ReceiverID = pendingResponse->receiverId;
  result.ChannelNumber = channelNo;
  result.Command = pendingResponse->command;
  if (command >= 0) {
    result.Command = command;
  }

  result.Result = resultId;
  result.DataSize = dataSize;
  if (dataSize > SUPLA_CALCFG_DATA_MAXSIZE) {
    SUPLA_LOG_WARNING("Data size %d is too big", dataSize);
    dataSize = SUPLA_CALCFG_DATA_MAXSIZE;
  }
  memcpy(result.Data, data, dataSize);
  SUPLA_LOG_DEBUG("Sending CALCFG result: CMD %d result: %d", result.Command,
                  result.Result);
  srpc_ds_async_device_calcfg_result(srpc, &result);
}

void Supla::Protocol::SuplaSrpc::clearPendingCalCfgResult(int16_t channelNo) {
  calCfgResultPending.clear(channelNo);
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
  srpcParams.user_params = this;

  srpc = srpc_init(&srpcParams);

  // Set Supla protocol interface version
  srpc_set_proto_version(srpc, version);
}

void Supla::Protocol::SuplaSrpc::deinitializeSrpc() {
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
