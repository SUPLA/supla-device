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

#include <string.h>

#include <supla/log_wrapper.h>

#include "SuplaDevice.h"
#include "supla/version.h"
#include "supla-common/log.h"
#include "supla-common/srpc.h"
#include "supla/channel.h"
#include "supla/device/last_state_logger.h"
#include "supla/element.h"
#include "supla/io.h"
#include "supla/network/network.h"
#include "supla/network/web_server.h"
#include "supla/storage/storage.h"
#include "supla/timer.h"
#include "supla/time.h"
#include "supla/storage/config.h"
#include "supla/tools.h"
#include "supla/actions.h"
#include "supla/device/sw_update.h"

void SuplaDeviceClass::status(int newStatus, const char *msg, bool alwaysLog) {
  bool showLog = false;

  if (currentStatus == STATUS_CONFIG_MODE
      && newStatus != STATUS_SOFTWARE_RESET
      && newStatus != STATUS_INVALID_GUID
      && newStatus != STATUS_INVALID_AUTHKEY) {
    // Config mode is final state and the only exit goes through reset
    // with exception for invalid GUID and AUTHKEY
    return;
  }

  if (currentStatus != newStatus) {
    if (!((newStatus == STATUS_SERVER_DISCONNECTED ||
            newStatus == STATUS_REGISTER_IN_PROGRESS)
          && currentStatus > STATUS_REGISTER_IN_PROGRESS)) {
      if (impl_arduino_status != nullptr) {
        impl_arduino_status(newStatus, msg);
      }
      currentStatus = newStatus;
      showLog = true;
      if (newStatus != STATUS_INITIALIZED) {
        addLastStateLog(msg);
      }
    }
  }
  if (alwaysLog || showLog) {
    SUPLA_LOG_INFO("Current status: [%d] %s", newStatus, msg);
  }
}

char *SuplaDeviceClass::getLastStateLog() {
  if (lastStateLogger) {
    return lastStateLogger->getLog();
  }
  return nullptr;
}

bool SuplaDeviceClass::prepareLastStateLog() {
  if (lastStateLogger) {
    return lastStateLogger->prepareLastStateLog();
  }
  return false;
}

SuplaDeviceClass::SuplaDeviceClass() {}

SuplaDeviceClass::~SuplaDeviceClass() {}

void SuplaDeviceClass::setStatusFuncImpl(
    _impl_arduino_status impl_arduino_status) {
  this->impl_arduino_status = impl_arduino_status;
}

bool SuplaDeviceClass::isSrpcInitialized(bool msg) {
  if (srpc != nullptr) {
    if (msg)
      status(STATUS_ALREADY_INITIALIZED, "SuplaDevice is already initialized");

    return true;
  }

  return false;
}

bool SuplaDeviceClass::begin(const char GUID[SUPLA_GUID_SIZE],
                             const char *Server,
                             const char *email,
                             const char authkey[SUPLA_AUTHKEY_SIZE],
                             unsigned char version) {
  setGUID(GUID);
  setServer(Server);
  setEmail(email);
  setAuthKey(authkey);

  return begin(version);
}

bool SuplaDeviceClass::begin(unsigned char version) {
  isSrpcInitialized(true);
  if (initializationDone) {
    return false;
  }

  SUPLA_LOG_DEBUG("Supla - starting initialization");


  Supla::Storage::Init();

  if (Supla::Storage::IsConfigStorageAvailable()) {
    if (!lastStateLogger) {
      lastStateLogger = new Supla::Device::LastStateLogger();
    }
    addFlags(SUPLA_DEVICE_FLAG_CALCFG_ENTER_CFG_MODE);
    loadDeviceConfig();

    // Load elements configuration
    for (auto element = Supla::Element::begin(); element != nullptr;
        element = element->next()) {
      element->onLoadConfig();
      delay(0);
    }
  }

  auto cfg = Supla::Storage::ConfigInstance();

  // Pefrorm dry run of write state to validate stored state section with
  // current device configuration
  if (Supla::Storage::PrepareState(true)) {
    SUPLA_LOG_DEBUG(
        "Validating storage state section with current device configuration");
    for (auto element = Supla::Element::begin(); element != nullptr;
         element = element->next()) {
      element->onSaveState();
      delay(0);
    }
    // If state storage validation was successful, perform read state
    if (Supla::Storage::FinalizeSaveState()) {
      SUPLA_LOG_INFO(
          "Storage state section validation completed. Loading elements "
          "state...");
      // Iterate all elements and load state
      Supla::Storage::PrepareState();
      for (auto element = Supla::Element::begin(); element != nullptr;
           element = element->next()) {
        element->onLoadState();
        delay(0);
      }
    }
  }

  // Initialize elements
  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onInit();
    delay(0);
  }

  // Enable timers
  Supla::initTimers();

  // Core elements initialization is done
  initializationDone = true;

  if (Supla::Network::Instance() == nullptr) {
    status(STATUS_MISSING_NETWORK_INTERFACE, "Network Interface not defined!");
    return false;
  }
  Supla::Network::Instance()->setSuplaDeviceClass(this);
  if (Supla::WebServer::Instance()) {
    Supla::WebServer::Instance()->setSuplaDeviceClass(this);
  }

  bool generateGuidAndAuthkey = false;
  if (isArrayEmpty(Supla::Channel::reg_dev.GUID, SUPLA_GUID_SIZE)) {
    // when config storage is available, GUID will be generated automatically
    if (!Supla::Storage::IsConfigStorageAvailable()) {
      status(STATUS_INVALID_GUID, "Missing GUID");
      return false;
    } else {
      generateGuidAndAuthkey = true;
    }
  }

  if (isArrayEmpty(Supla::Channel::reg_dev.AuthKey, SUPLA_AUTHKEY_SIZE)) {
    // when config storage is available, AuthKey will be generated automatically
    if (!Supla::Storage::IsConfigStorageAvailable()) {
      status(STATUS_INVALID_AUTHKEY, "Missing AuthKey");
      return false;
    } else {
      generateGuidAndAuthkey = true;
    }
  }

  if (Supla::Channel::reg_dev.ServerName[0] == '\0') {
    status(STATUS_UNKNOWN_SERVER_ADDRESS, "Missing server address");
    if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
      return false;
    }
  }

  if (Supla::Channel::reg_dev.Email[0] == '\0') {
    status(STATUS_MISSING_CREDENTIALS, "Missing email address");
    if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
      return false;
    }
  }

  if (strnlen(Supla::Channel::reg_dev.Name, SUPLA_DEVICE_NAME_MAXSIZE) == 0) {
#if defined(ARDUINO_ARCH_ESP8266)
    setName("SUPLA-ESP8266");
#elif defined(ARDUINO_ARCH_ESP32)
    setName("SUPLA-ESP32");
#elif defined(ARDUINO_ARCH_AVR)
    setName("SUPLA-ARDUINO");
#else
    setName("SUPLA-DEVICE");
#endif
  }

  if (strnlen(Supla::Channel::reg_dev.SoftVer, SUPLA_SOFTVER_MAXSIZE) == 0) {
    setSwVersion(suplaDeviceVersion);
  }

  SUPLA_LOG_DEBUG("Initializing network layer");
  char hostname[32] = {};
  generateHostname(hostname, 6);
  Supla::Network::SetHostname(hostname);
  if (cfg) {
    uint8_t securityLevel = 0;
    cfg->getUInt8("security_level", &securityLevel);
    if (securityLevel > 2) {
      securityLevel = 0;
    }
    auto network = Supla::Network::Instance();
    network->setSSLEnabled(true);
    SUPLA_LOG_DEBUG("Security level: %d", securityLevel);
    switch (securityLevel) {
      default:
      case 0: {
        // in case of default security level it is required to use Supla CA
        // certificate. It should be set on application level before
        // SuplaDevice.begin() is called.
        // If it is null, we just assign "SUPLA" as a certificate value, which
        // will of course fail the certificate validation (which is intended).
        if (suplaCACert == nullptr) {
          SUPLA_LOG_ERROR(
            "Supla CA ceritificate is selected, but it is not set. Connection "
            "will fail");
          auto cert = new char[6];
          strncpy(cert, "SUPLA", 6);
          suplaCACert = cert;
        }
        network->setCACert(suplaCACert);
        break;
      }
      case 1: {
        // custom CA from Config
        int len = cfg->getCustomCASize();
        if (len > 0) {
          len++;
          auto cert = new char[len];
          cfg->getCustomCA(cert, len);
          network->setCACert(cert);
        } else {
          SUPLA_LOG_ERROR("Custom CA is selected, but certificate is"
              " missing in config. Connect will fail");
          auto cert = new char[6];
          strncpy(cert, "SUPLA", 6);
          network->setCACert(cert);
        }
        break;
      }
      case 2: {
        // Skip certificate validation (INSECURE)
        break;
      }
    }
  }
  if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
    enterConfigMode();
  } else {
    enterNormalMode();
  }

  TsrpcParams srpc_params;
  srpc_params_init(&srpc_params);
  srpc_params.data_read = &Supla::data_read;
  srpc_params.data_write = &Supla::data_write;
  srpc_params.on_remote_call_received = &Supla::message_received;
  srpc_params.user_params = this;

  srpc = srpc_init(&srpc_params);

  // Set Supla protocol interface version
  srpc_set_proto_version(srpc, version);

  SUPLA_LOG_INFO("Using Supla protocol version %d", version);

  if (generateGuidAndAuthkey) {
    if (cfg && cfg->generateGuidAndAuthkey()) {
      SUPLA_LOG_INFO("Successfully generated GUID and AuthKey");
      char buf[512] = {};
      if (cfg->getGUID(buf)) {
        setGUID(buf);
      }
      if (cfg->getAuthKey(buf)) {
        setAuthKey(buf);
      }
      generateHexString(Supla::Channel::reg_dev.GUID, buf, SUPLA_GUID_SIZE);
      SUPLA_LOG_INFO("New GUID: %s", buf);
      generateHexString(Supla::Channel::reg_dev.AuthKey, buf,
          SUPLA_AUTHKEY_SIZE);
      SUPLA_LOG_DEBUG("New AuthKey: %s", buf);
    } else {
      SUPLA_LOG_ERROR("Failed to generate GUID and AuthKey");
      status(STATUS_INVALID_GUID, "Missing GUID");
      status(STATUS_INVALID_AUTHKEY, "Missing AuthKey");
      return false;
    }
  }

  status(STATUS_INITIALIZED, "SuplaDevice initialized");
  return true;
}

void SuplaDeviceClass::setName(const char *Name) {
  setString(Supla::Channel::reg_dev.Name, Name, SUPLA_DEVICE_NAME_MAXSIZE);
}

void SuplaDeviceClass::setString(char *dst, const char *src, int max_size) {
  if (src == nullptr) {
    dst[0] = 0;
    return;
  }

  int size = strlen(src);

  if (size + 1 > max_size) size = max_size - 1;

  strncpy(dst, src, size);
  dst[size] = 0;
}

void SuplaDeviceClass::onTimer(void) {
  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onTimer();
  }
}

void SuplaDeviceClass::onFastTimer(void) {
  // Iteration over all impulse counters will count incomming impulses. It is
  // after SuplaDevice initialization (because we have to read stored counter
  // values) and before any other operation like connection to Supla cloud
  // (because we want to count impulses even when we have connection issues.
  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onFastTimer();
  }
}

void SuplaDeviceClass::iterate(void) {
  if (!initializationDone) {
    return;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->saveIfNeeded();
  }

  uint64_t _millis = millis();
  checkIfRestartIsNeeded(_millis);
  handleLocalActionTriggers();
  iterateAlwaysElements(_millis);

  if (!isSrpcInitialized(false)) {
    return;
  }

  if (forceRestartTimeMs) {
    return;
  }
  if (waitForIterate != 0 && _millis - lastIterateTime < waitForIterate) {
    return;
  }

  waitForIterate = 0;

  bool enterIterateConnected = false;
  if (_millis - lastIterateTime > 0) {
    enterIterateConnected = true;
  }

  // Wait for registration (timeout) use lastIterateTime, so we don't change
  // it here if we're waiting for registration reply
  if (registered != -1) {
    lastIterateTime = _millis;
  }

  switch (deviceMode) {
    default: {
      if (!iterateNetworkSetup()) {
        registered = 0;
        return;
      }

      // Establish connection with Supla server
      if (!Supla::Network::Connected()) {
        uptime.setConnectionLostCause(
            SUPLA_LASTCONNECTIONRESETCAUSE_SERVER_CONNECTION_LOST);
        registered = 0;
        int result =
          Supla::Network::Connect(Supla::Channel::reg_dev.ServerName, port);
        if (1 == result) {
          uptime.resetConnectionUptime();
          connectionFailCounter = 0;
          lastConnectionResetCounter = 0;
          SUPLA_LOG_INFO("Connected to Supla Server");

        } else {
          status(STATUS_SERVER_DISCONNECTED, "Not connected to Supla server");
          SUPLA_LOG_DEBUG("Connection fail (%d). Server: %s",
              result,
              Supla::Channel::reg_dev.ServerName);
          Supla::Network::Disconnect();
          waitForIterate = 10000;
          connectionFailCounter++;
          return;
        }
      }

      // Handle communication with Supla server and maintain link
      if (iterateSuplaProtocol(_millis) && enterIterateConnected) {
        // Iterate all elements
        for (auto element = Supla::Element::begin(); element != nullptr;
            element = element->next()) {
          if (!element->iterateConnected(srpc)) {
            break;
          }
          delay(0);
        }
      }
      break;
    }
    case Supla::DEVICE_MODE_CONFIG: {
      break;
    }
    case Supla::DEVICE_MODE_SW_UPDATE: {
      if (!iterateNetworkSetup()) {
        return;
      }

      auto cfg = Supla::Storage::ConfigInstance();
      if (swUpdate == nullptr) {
        status(STATUS_SW_DOWNLOAD, "SW update in progress...");
        char url[SUPLA_MAX_URL_LENGTH] = {};
        if (cfg) {
          cfg->getSwUpdateServer(url);
        }
        if (strlen(url) == 0) {
          swUpdate = Supla::Device::SwUpdate::Create(this,
              "http://update.supla.org");
        } else {
          swUpdate = Supla::Device::SwUpdate::Create(this, url);
        }
        if (cfg->isSwUpdateBeta()) {
          swUpdate->useBeta();
        }
      }
      if (!swUpdate->isStarted()) {
        SUPLA_LOG_INFO("Starting SW update");
        Supla::Network::Disconnect();
        swUpdate->start();
      } else {
        swUpdate->iterate();
        if (swUpdate->isAborted()) {
          SUPLA_LOG_INFO("SW update aborted");
          delete swUpdate;
          swUpdate = nullptr;
        } else if (swUpdate->isFinished()) {
          SUPLA_LOG_INFO("Finished SW update, restarting...");
          delete swUpdate;
          swUpdate = nullptr;
          scheduleSoftRestart();
        }
        if (swUpdate == nullptr) {
          deviceMode = Supla::DEVICE_MODE_NORMAL;
          if (cfg) {
            cfg->setDeviceMode(Supla::DEVICE_MODE_NORMAL);
            cfg->setSwUpdateBeta(false);
            cfg->commit();
          }
        }
      }

      break;
    }
  }
}

void SuplaDeviceClass::onVersionError(TSDC_SuplaVersionError *version_error) {
  status(STATUS_PROTOCOL_VERSION_ERROR, "Protocol version error");
  SUPLA_LOG_ERROR(
      "Protocol version error. Server min: %d; Server version: %d",
      version_error->server_version_min,
      version_error->server_version);

  Supla::Network::Disconnect();

  lastIterateTime = millis();
  waitForIterate = 15000;
}

void SuplaDeviceClass::onRegisterResult(
    TSD_SuplaRegisterDeviceResult *register_device_result) {
  _supla_int_t activity_timeout = 0;

  switch (register_device_result->result_code) {
    // OK scenario
    case SUPLA_RESULTCODE_TRUE:
      activity_timeout = register_device_result->activity_timeout;
      Supla::Network::Instance()->setActivityTimeout(activity_timeout);
      registered = 1;
      SUPLA_LOG_DEBUG(
          "Device registered (activity timeout %d s, server version: %d, "
          "server min version: %d)",
          register_device_result->activity_timeout,
          register_device_result->version,
          register_device_result->version_min);
      lastIterateTime = millis();
      status(STATUS_REGISTERED_AND_READY, "Registered and ready");

      if (activity_timeout != activityTimeout) {
        SUPLA_LOG_DEBUG("Changing activity timeout to %d", activityTimeout);
        TDCS_SuplaSetActivityTimeout at;
        at.activity_timeout = activityTimeout;
        srpc_dcs_async_set_activity_timeout(srpc, &at);
      }

      for (auto element = Supla::Element::begin(); element != nullptr;
          element = element->next()) {
        element->onRegistered();
        delay(0);
      }

      return;

      // NOK scenarios
    case SUPLA_RESULTCODE_TEMPORARILY_UNAVAILABLE:
      status(STATUS_TEMPORARILY_UNAVAILABLE, "Temporarily unavailable!", true);
      break;

    case SUPLA_RESULTCODE_GUID_ERROR:
      status(STATUS_INVALID_GUID, "Incorrect device GUID!", true);
      break;

    case SUPLA_RESULTCODE_AUTHKEY_ERROR:
      status(STATUS_INVALID_AUTHKEY, "Incorrect AuthKey!", true);
      break;

    case SUPLA_RESULTCODE_BAD_CREDENTIALS:
      status(STATUS_BAD_CREDENTIALS,
          "Bad credentials - incorrect AuthKey or email", true);
      break;

    case SUPLA_RESULTCODE_REGISTRATION_DISABLED:
      status(STATUS_REGISTRATION_DISABLED, "Registration disabled!", true);
      break;

    case SUPLA_RESULTCODE_DEVICE_LIMITEXCEEDED:
      status(STATUS_DEVICE_LIMIT_EXCEEDED, "Device limit exceeded!", true);
      break;

    case SUPLA_RESULTCODE_NO_LOCATION_AVAILABLE:
      status(STATUS_NO_LOCATION_AVAILABLE, "No location available!", true);
      break;

    case SUPLA_RESULTCODE_DEVICE_DISABLED:
      status(STATUS_DEVICE_IS_DISABLED, "Device is disabled!", true);
      break;

    case SUPLA_RESULTCODE_LOCATION_DISABLED:
      status(STATUS_LOCATION_IS_DISABLED, "Location is disabled!", true);
      break;

    case SUPLA_RESULTCODE_LOCATION_CONFLICT:
      status(STATUS_LOCATION_CONFLICT, "Location conflict!", true);
      break;

    case SUPLA_RESULTCODE_CHANNEL_CONFLICT:
      status(STATUS_CHANNEL_CONFLICT, "Channel conflict!", true);
      break;

    default:
      status(STATUS_UNKNOWN_ERROR, "Unknown registration error", true);
      SUPLA_LOG_ERROR(
          "Register result code %i",
          register_device_result->result_code);
      break;
  }

  Supla::Network::Disconnect();
  // server rejected registration
  registered = 2;
}

void SuplaDeviceClass::channelSetActivityTimeoutResult(
    TSDC_SuplaSetActivityTimeoutResult *result) {
  Supla::Network::Instance()->setActivityTimeout(result->activity_timeout);
  SUPLA_LOG_DEBUG("Activity timeout set to %d s", result->activity_timeout);
}

void SuplaDeviceClass::setServerPort(int value) {
  port = value;
}

void SuplaDeviceClass::setSwVersion(const char *swVersion) {
  setString(Supla::Channel::reg_dev.SoftVer, swVersion, SUPLA_SOFTVER_MAXSIZE);
}

int SuplaDeviceClass::getCurrentStatus() {
  return currentStatus;
}

void SuplaDeviceClass::fillStateData(TDSC_ChannelState *channelState) {
  channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_UPTIME |
                         SUPLA_CHANNELSTATE_FIELD_CONNECTIONUPTIME;

  channelState->Uptime = uptime.getUptime();
  channelState->ConnectionUptime = uptime.getConnectionUptime();
  if (uptime.getLastResetCause() > 0) {
    channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_LASTCONNECTIONRESETCAUSE;
    channelState->LastConnectionResetCause = uptime.getLastResetCause();
  }
}

void SuplaDeviceClass::setGUID(const char GUID[SUPLA_GUID_SIZE]) {
  memcpy(Supla::Channel::reg_dev.GUID, GUID, SUPLA_GUID_SIZE);
}

void SuplaDeviceClass::setAuthKey(const char authkey[SUPLA_AUTHKEY_SIZE]) {
  memcpy(Supla::Channel::reg_dev.AuthKey, authkey, SUPLA_AUTHKEY_SIZE);
}

void SuplaDeviceClass::setEmail(const char *email) {
  setString(Supla::Channel::reg_dev.Email, email, SUPLA_EMAIL_MAXSIZE);
}

void SuplaDeviceClass::setServer(const char *server) {
  setString(
      Supla::Channel::reg_dev.ServerName, server, SUPLA_SERVER_NAME_MAXSIZE);
}

void SuplaDeviceClass::onGetUserLocaltimeResult(
    TSDC_UserLocalTimeResult *result) {
  if (clock) {
    clock->parseLocaltimeFromServer(result);
  }
}

void SuplaDeviceClass::addClock(Supla::Clock *_clock) {
  SUPLA_LOG_DEBUG("Clock class added");
  clock = _clock;
}

Supla::Clock *SuplaDeviceClass::getClock() {
  return clock;
}

void SuplaDeviceClass::loadDeviceConfig() {
  auto cfg = Supla::Storage::ConfigInstance();

  bool configIncomplete = false;
  char buf[256] = {};

  // Device generic config
  memset(buf, 0, sizeof(buf));
  if (cfg->getDeviceName(buf)) {
    setName(buf);
  }

  memset(buf, 0, sizeof(buf));
  if (cfg->getGUID(buf)) {
    setGUID(buf);
  }

  deviceMode = cfg->getDeviceMode();
  if (deviceMode == Supla::DEVICE_MODE_NOT_SET) {
    deviceMode = Supla::DEVICE_MODE_NORMAL;
  }

  // Supla protocol specific config
  if (cfg->isSuplaCommProtocolEnabled()) {
    memset(buf, 0, sizeof(buf));
    if (cfg->getSuplaServer(buf) && strlen(buf) > 0) {
      setServer(buf);
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing server");
      configIncomplete = true;
    }
    setServerPort(cfg->getSuplaServerPort());

    memset(buf, 0, sizeof(buf));
    if (cfg->getEmail(buf) && strlen(buf) > 0) {
      setEmail(buf);
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing email");
      configIncomplete = true;
    }

    memset(buf, 0, sizeof(buf));
    if (cfg->getAuthKey(buf)) {
      setAuthKey(buf);
    }
  }

  // MQTT protocol specific config
  if (cfg->isMqttCommProtocolEnabled()) {
    // TODO(klew): add MQTT config
    addLastStateLog("MQTT protocol is not supported yet");
    configIncomplete = true;
    SUPLA_LOG_ERROR("MQTT support not implemented yet");
  }

  // WiFi specific config
  auto net = Supla::Network::Instance();

  if (net != nullptr && net->isWifiConfigRequired()) {
    memset(buf, 0, sizeof(buf));
    if (cfg->getWiFiSSID(buf) && strlen(buf) > 0) {
      net->setSsid(buf);
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing Wi-Fi SSID");
      addLastStateLog("Missing Wi-Fi SSID");
      configIncomplete = true;
    }

    memset(buf, 0, sizeof(buf));
    if (cfg->getWiFiPassword(buf) && strlen(buf) > 0) {
      net->setPassword(buf);
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing Wi-Fi password");
      addLastStateLog("Missing Wi-Fi password");
      configIncomplete = true;
    }
  }

  if (configIncomplete) {
    SUPLA_LOG_INFO("Config incomplete: deviceMode = CONFIG");
    deviceMode = Supla::DEVICE_MODE_CONFIG;
  }
}

void SuplaDeviceClass::iterateAlwaysElements(uint64_t _millis) {
  uptime.iterate(_millis);

  // Iterate all elements
  for (auto element = Supla::Element::begin(); element != nullptr;
      element = element->next()) {
    element->iterateAlways();
    delay(0);
  }

  // Iterate all elements and saves state
  if (Supla::Storage::SaveStateAllowed(_millis)) {
    saveStateToStorage();
  }
}


bool SuplaDeviceClass::iterateNetworkSetup() {
  // Restart network after >1 min of failed connection attempts
  if (networkIsNotReadyCounter == 0 &&
      lastConnectionResetCounter != connectionFailCounter &&
      connectionFailCounter > 0 &&
      connectionFailCounter % 6 == 0) {
    lastConnectionResetCounter = connectionFailCounter;
    SUPLA_LOG_WARNING(
        "Connection fail counter overflow. Trying to setup network "
        "interface again");
    Supla::Network::Setup();
  }

  if (!Supla::Network::IsReady()) {
    if (connectionFailCounter > 0 &&
        lastConnectionResetCounter != connectionFailCounter) {
      status(STATUS_NETWORK_DISCONNECTED, "No connection to network");
      uptime.setConnectionLostCause(
          SUPLA_LASTCONNECTIONRESETCAUSE_WIFI_CONNECTION_LOST);
    }
    waitForIterate = 100;
    networkIsNotReadyCounter++;
    // We wait 10s before we increment "connectionFailCounter".
    // This has to be aligned with waitForIterate timeout for establishing
    // connection with server when network isReady() == true.
    if (networkIsNotReadyCounter > 100) {
      networkIsNotReadyCounter = 0;
      connectionFailCounter++;
      SUPLA_LOG_DEBUG("Conn fail counter %d", connectionFailCounter);
    }
    return false;
  }

  networkIsNotReadyCounter = 0;
  Supla::Network::Iterate();
  return true;
}

bool SuplaDeviceClass::iterateSuplaProtocol(uint64_t _millis) {
  if (srpc_iterate(srpc) == SUPLA_RESULT_FALSE) {
    status(STATUS_ITERATE_FAIL, "Communication failure");
    Supla::Network::Disconnect();

    lastIterateTime = _millis;
    waitForIterate = 5000;
    return false;
  }

  if (registered == 0) {
    // Perform registration if we are not yet registered
    registered = -1;
    status(STATUS_REGISTER_IN_PROGRESS, "Register in progress");
    if (!srpc_ds_async_registerdevice_e(srpc, &Supla::Channel::reg_dev)) {
      SUPLA_LOG_WARNING("Fatal SRPC failure!");
    }
    return false;
  } else if (registered == -1) {
    // Handle registration timeout (in case of no reply received)
    if (_millis - lastIterateTime > 10*1000) {
      SUPLA_LOG_DEBUG(
          "No reply to registration message. Resetting connection.");
      status(STATUS_SERVER_DISCONNECTED, "Not connected to Supla server");
      Supla::Network::Disconnect();

      lastIterateTime = _millis;
      waitForIterate = 2000;
    }
    return false;
  } else if (registered == 1) {
    // Device is registered and everything is correct

    if (Supla::Network::Ping(srpc) == false) {
      uptime.setConnectionLostCause(
          SUPLA_LASTCONNECTIONRESETCAUSE_ACTIVITY_TIMEOUT);
      SUPLA_LOG_DEBUG("TIMEOUT - lost connection with server");
      status(STATUS_SERVER_DISCONNECTED, "Not connected to Supla server");
      Supla::Network::Disconnect();
    }

    return true;
  } else if (registered == 2) {
    // Server rejected registration
    registered = 0;
    lastIterateTime = millis();
    waitForIterate = 10000;
  }
  return false;
}

void SuplaDeviceClass::enterConfigMode() {
  if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
    // if we enter cfg mode with deviceMode already set to cfgmode, then
    // configuration is incomplete, so there is no timeout to leave config
    // mode
    deviceRestartTimeoutTimestamp = 0;
  } else {
    deviceRestartTimeoutTimestamp = millis();
  }

  deviceMode = Supla::DEVICE_MODE_CONFIG;
  Supla::Network::Disconnect();
  Supla::Network::SetConfigMode();
  Supla::Network::Setup();

  if (Supla::WebServer::Instance()) {
    Supla::WebServer::Instance()->start();
  }
  status(STATUS_CONFIG_MODE, "Config mode", true);
}

void SuplaDeviceClass::softRestart() {
  status(STATUS_SOFTWARE_RESET, "Software reset");
  saveStateToStorage();
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->commit();
  }
  deviceMode = Supla::DEVICE_MODE_NORMAL;

  // TODO(klew): stop supla timers

  if (Supla::WebServer::Instance()) {
    Supla::WebServer::Instance()->stop();
  }

  Supla::Network::Uninit();
  SUPLA_LOG_INFO("Resetting in 0.5s...");
  delay(500);
  SUPLA_LOG_INFO("See you soon!");
  deviceSoftwareReset();
}

void SuplaDeviceClass::enterNormalMode() {
  SUPLA_LOG_INFO("Enter normal mode");
  Supla::Network::SetNormalMode();
  Supla::Network::Setup();
}

void SuplaDeviceClass::setManufacurerId(_supla_int16_t id) {
  Supla::Channel::reg_dev.ManufacturerID = id;
}

void SuplaDeviceClass::setProductId(_supla_int16_t id) {
  Supla::Channel::reg_dev.ProductID = id;
}

void SuplaDeviceClass::addFlags(_supla_int_t newFlags) {
  Supla::Channel::reg_dev.Flags |= newFlags;
}

void SuplaDeviceClass::removeFlags(_supla_int_t flags) {
  Supla::Channel::reg_dev.Flags &= ~flags;
}

int SuplaDeviceClass::handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) {
  if (request) {
    switch (request->Command) {
      case SUPLA_CALCFG_CMD_ENTER_CFG_MODE: {
        SUPLA_LOG_INFO("CALCFG ENTER CFGMODE received");
        enterConfigMode();
        return SUPLA_CALCFG_RESULT_DONE;
      }
      default:
        break;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

void SuplaDeviceClass::saveStateToStorage() {
  Supla::Storage::PrepareState();
  for (auto element = Supla::Element::begin(); element != nullptr;
      element = element->next()) {
    element->onSaveState();
    delay(0);
  }
  Supla::Storage::FinalizeSaveState();
}

int SuplaDeviceClass::generateHostname(char *buf, int macSize) {
  char name[32] = {};
  if (macSize < 0) {
    macSize = 0;
  }
  if (macSize > 6) {
    macSize = 6;
  }

  int appendixSize = 0;
  if (macSize > 0) {
    appendixSize = 1 + 2*macSize;
  }

  char *srcName = Supla::Channel::reg_dev.Name;

  int nameLength = strlen(Supla::Channel::reg_dev.Name);

  if (nameLength + appendixSize > 31) {
    nameLength = 31 - appendixSize;
  }

  if (nameLength == 0) {
    setName("SUPLA-DEVICE");
    nameLength = strlen(Supla::Channel::reg_dev.Name);
  }

  int destIdx = 0;
  for (int i = 0; i < nameLength; i++) {
    if (srcName[i] < 32) {
      continue;
    } else if (srcName[i] < 48) {
      name[destIdx++] = '-';
    } else if (srcName[i] < 58) {
      name[destIdx++] = srcName[i];  // copy numbers
    } else if (srcName[i] < 65) {
      name[destIdx++] = '-';
    } else if (srcName[i] < 91) {
      name[destIdx++] = srcName[i];  // copy capital chars
    } else if (srcName[i] < 97) {
      name[destIdx++] = '-';
    } else if (srcName[i] < 123) {
      name[destIdx++] = srcName[i] - 32;  // capitalize small chars
    }
  }

  if (macSize > 0) {
    uint8_t mac[6] = {};
    if (Supla::Network::GetMacAddr(mac)) {
      name[destIdx++] = '-';
      destIdx += generateHexString(mac + (6 - macSize), &(name[destIdx]),
          macSize);
    }
  }

  name[destIdx++] = 0;
  strncpy(buf, name, 32);

  return destIdx;
}

void SuplaDeviceClass::disableCfgModeTimeout() {
  if (!forceRestartTimeMs) {
    deviceRestartTimeoutTimestamp = 0;
  }
}

void SuplaDeviceClass::scheduleSoftRestart(int timeout) {
  SUPLA_LOG_INFO("Triggering soft restart");
  if (timeout <= 0) {
    forceRestartTimeMs = 1;
  } else {
    forceRestartTimeMs = timeout;
  }
  deviceRestartTimeoutTimestamp = millis();
}

void SuplaDeviceClass::addLastStateLog(const char *msg) {
  if (lastStateLogger) {
    lastStateLogger->log(msg);
  }
}

void SuplaDeviceClass::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case Supla::SOFT_RESTART: {
      scheduleSoftRestart(0);
      break;
    }
    case Supla::ENTER_CONFIG_MODE: {
      if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
        goToConfigModeAsap = true;
      }
      break;
    }
    case Supla::TOGGLE_CONFIG_MODE: {
      if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
        goToConfigModeAsap = true;
      } else {
        scheduleSoftRestart(0);
      }
      break;
    }
    case Supla::RESET_TO_FACTORY_SETTINGS: {
      triggerResetToFacotrySettings = true;;
      break;
    }
    case Supla::START_LOCAL_WEB_SERVER: {
      triggerStartLocalWebServer = true;;
      break;
    }
    case Supla::STOP_LOCAL_WEB_SERVER: {
      triggerStopLocalWebServer = true;;
      break;
    }
    case Supla::CHECK_SW_UPDATE: {
      if (deviceMode != Supla::DEVICE_MODE_SW_UPDATE) {
        triggerCheckSwUpdate = true;
      }
      break;
    }
    case Supla::ENTER_CONFIG_MODE_OR_RESET_TO_FACTORY: {
      if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
        goToConfigModeAsap = true;
      } else {
        resetToFactorySettings();
        scheduleSoftRestart(0);
      }
      break;
    }
    case Supla::LEAVE_CONFIG_MODE_AND_RESET: {
      if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
        // TODO(klew): add check if configuration is complete, otherwise
        // don't allow to reset
        scheduleSoftRestart(0);
      }
      break;
    }
  }
}

void SuplaDeviceClass::resetToFactorySettings() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->removeAll();
    cfg->setGUID(Supla::Channel::reg_dev.GUID);
    cfg->setAuthKey(Supla::Channel::reg_dev.AuthKey);
    cfg->commit();
  }
}

void SuplaDeviceClass::handleLocalActionTriggers() {
  if (goToConfigModeAsap) {
    goToConfigModeAsap = false;
    enterConfigMode();
    disableCfgModeTimeout();
  }

  if (triggerResetToFacotrySettings) {
    resetToFactorySettings();
    softRestart();
  }

  if (triggerStartLocalWebServer) {
    triggerStartLocalWebServer = false;
    if (Supla::WebServer::Instance()) {
      Supla::WebServer::Instance()->start();
    }
  }

  if (triggerStopLocalWebServer) {
    triggerStopLocalWebServer = false;
    if (Supla::WebServer::Instance()) {
      Supla::WebServer::Instance()->stop();
    }
  }

  if (triggerCheckSwUpdate) {
    triggerCheckSwUpdate = false;
    if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
      deviceMode = Supla::DEVICE_MODE_SW_UPDATE;
    }
  }
}

void SuplaDeviceClass::checkIfRestartIsNeeded(uint64_t _millis) {
  if (deviceRestartTimeoutTimestamp != 0 &&
      _millis - deviceRestartTimeoutTimestamp > 5ul * 60 * 1000) {
    SUPLA_LOG_INFO("Config mode 5 min timeout. Reset device");
    softRestart();
  }
  if (forceRestartTimeMs &&
      _millis - deviceRestartTimeoutTimestamp > forceRestartTimeMs) {
    SUPLA_LOG_DEBUG("Reset requested. Reset device");
    softRestart();
  }
  if (resetOnConnectionFailCounter > 6 &&
      connectionFailCounter == resetOnConnectionFailCounter) {
    SUPLA_LOG_INFO("Connection fail counter overflow - software reset");
    softRestart();
  }
}

const uint8_t *SuplaDeviceClass::getRsaPublicKey() {
  return rsaPublicKey;
}

void SuplaDeviceClass::setRsaPublicKeyPtr(const uint8_t *ptr) {
  rsaPublicKey = ptr;
}

void SuplaDeviceClass::setAutomaticResetOnConnectionProblem(
    unsigned int timeSec) {
  resetOnConnectionFailCounter = timeSec / 10;
}

void SuplaDeviceClass::setLastStateLogger(
    Supla::Device::LastStateLogger *logger) {
  lastStateLogger = logger;
}

void SuplaDeviceClass::setActivityTimeout(_supla_int_t newActivityTimeout) {
  activityTimeout = newActivityTimeout;
}

void SuplaDeviceClass::setSuplaCACert(const char *cert) {
  suplaCACert = cert;
}

SuplaDeviceClass SuplaDevice;
