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

#include "SuplaDevice.h"

#include <stdio.h>
#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/actions.h>
#include <supla/channel.h>
#include <supla/device/last_state_logger.h>
#include <supla/device/sw_update.h>
#include <supla/element.h>
#include <supla/events.h>
#include <supla/io.h>
#include <supla/network/network.h>
#include <supla/network/web_server.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/timer.h>
#include <supla/tools.h>
#include <supla/version.h>

void SuplaDeviceClass::status(int newStatus, const char *msg, bool alwaysLog) {
  bool showLog = false;

  if ((currentStatus == STATUS_CONFIG_MODE ||
       currentStatus == STATUS_TEST_WAIT_FOR_CFG_BUTTON) &&
      newStatus != STATUS_SOFTWARE_RESET && newStatus != STATUS_INVALID_GUID &&
      newStatus != STATUS_INVALID_AUTHKEY) {
    // Config mode and testing is final state and the only exit goes through
    // reset with exception for invalid GUID and AUTHKEY
    return;
  }

  if (currentStatus != newStatus) {
    if (!(newStatus == STATUS_REGISTER_IN_PROGRESS &&
          currentStatus > STATUS_REGISTER_IN_PROGRESS)) {
      if (impl_arduino_status != nullptr) {
        impl_arduino_status(newStatus, msg);
      }
      currentStatus = newStatus;
      showLog = true;
      if (newStatus != STATUS_INITIALIZED && msg != nullptr) {
        addLastStateLog(msg);
      }
      runAction(Supla::ON_DEVICE_STATUS_CHANGE);
    }
  }
  if ((alwaysLog || showLog) && msg != nullptr) {
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

SuplaDeviceClass::SuplaDeviceClass() {
}

SuplaDeviceClass::~SuplaDeviceClass() {
  if (srpcLayer) {
    delete srpcLayer;
    srpcLayer = nullptr;
  }
  if (customHostnamePrefix) {
    delete[] customHostnamePrefix;
    customHostnamePrefix = nullptr;
  }
  if (lastStateLogger) {
    delete lastStateLogger;
    lastStateLogger = nullptr;
  }
}

void SuplaDeviceClass::setStatusFuncImpl(
    _impl_arduino_status impl_arduino_status) {
  this->impl_arduino_status = impl_arduino_status;
}

bool SuplaDeviceClass::begin(const char GUID[SUPLA_GUID_SIZE],
                             const char *Server,
                             const char *email,
                             const char authkey[SUPLA_AUTHKEY_SIZE],
                             unsigned char protoVersion) {
  setGUID(GUID);
  setAuthKey(authkey);
  setServer(Server);
  setEmail(email);

  return begin(protoVersion);
}

bool SuplaDeviceClass::begin(unsigned char protoVersion) {
  if (initializationDone) {
    status(STATUS_ALREADY_INITIALIZED, "SuplaDevice is already initialized");
    return false;
  }
  initializationDone = true;

  SUPLA_LOG_DEBUG("Supla - starting initialization");

  // Initialize protocol layers
  createSrpcLayerIfNeeded();
  srpcLayer->setVersion(protoVersion);

  storageInitResult = Supla::Storage::Init();

  bool atLeastOneProtoIsEnabled = false;
  bool configComplete = true;
  if (Supla::Storage::IsConfigStorageAvailable()) {
    if (!lastStateLogger) {
      lastStateLogger = new Supla::Device::LastStateLogger();
    }
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg->isConfigModeSupported()) {
      addFlags(SUPLA_DEVICE_FLAG_CALCFG_ENTER_CFG_MODE);
    }

    // Load device and network related configuration
    if (!loadDeviceConfig()) {
      configComplete = false;
    }

    // Load protocol layer configuration
    for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
         proto = proto->next()) {
      if (!proto->onLoadConfig()) {
        configComplete = false;
      }
      if (!proto->isConfigEmpty()) {
        configEmpty = false;
      }
      if (proto->isEnabled()) {
        atLeastOneProtoIsEnabled = true;
      }
      delay(0);
    }

    if (!atLeastOneProtoIsEnabled) {
      status(STATUS_ALL_PROTOCOLS_DISABLED,
             "All communication protocols are disabled");
      configComplete = false;
    }

    // Load elements configuration
    for (auto element = Supla::Element::begin(); element != nullptr;
         element = element->next()) {
      element->onLoadConfig(this);
      delay(0);
    }
  }

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

  if (!configComplete) {
    SUPLA_LOG_INFO("Config incomplete: deviceMode = CONFIG");
    deviceMode = Supla::DEVICE_MODE_CONFIG;
  }

  // Verify if configuration is complete for each protocol
  // Verification adds last state log. It returns false only in "normal mode".
  bool verificationSuccess = true;
  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
       proto = proto->next()) {
    if (!proto->verifyConfig()) {
      verificationSuccess = false;
    }
    delay(0);
  }

  if (verificationSuccess == false) {
    // this may happen only when Supla::Storage::Config is not used
    return false;
  }

  if (generateGuidAndAuthkey) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg && cfg->generateGuidAndAuthkey()) {
      SUPLA_LOG_INFO("Successfully generated GUID and AuthKey");
      char buf[512] = {};
      if (cfg->getGUID(buf)) {
        setGUID(buf);
      }
      if (cfg->getAuthKey(buf)) {
        setAuthKey(buf);
      }
      generateHexString(
          Supla::Channel::reg_dev.AuthKey, buf, SUPLA_AUTHKEY_SIZE);
      SUPLA_LOG_DEBUG("New AuthKey: %s", buf);
    } else {
      SUPLA_LOG_ERROR("Failed to generate GUID and AuthKey");
      status(STATUS_INVALID_GUID, "Missing GUID");
      status(STATUS_INVALID_AUTHKEY, "Missing AuthKey");
      return false;
    }
  }

  char buf[SUPLA_GUID_SIZE * 2 + 1] = {};
  generateHexString(Supla::Channel::reg_dev.GUID, buf, SUPLA_GUID_SIZE);
  SUPLA_LOG_INFO("GUID: %s", buf);

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
  SUPLA_LOG_INFO("Device name: %s", Supla::Channel::reg_dev.Name);
  SUPLA_LOG_INFO("Device software version: %s",
      Supla::Channel::reg_dev.SoftVer);

  SUPLA_LOG_DEBUG("Initializing network layer");
  char hostname[32] = {};
  generateHostname(hostname, 6);
  Supla::Network::SetHostname(hostname);

  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
       proto = proto->next()) {
    proto->onInit();
    delay(0);
  }

  status(STATUS_INITIALIZED, "SuplaDevice initialized");

  if ((allowOfflineMode == 1 && deviceMode == Supla::DEVICE_MODE_CONFIG &&
       configEmpty) ||
      (allowOfflineMode == 2 && deviceMode == Supla::DEVICE_MODE_CONFIG &&
       !atLeastOneProtoIsEnabled)) {
    deviceMode = Supla::DEVICE_MODE_NORMAL;
    SUPLA_LOG_INFO("Disabling network setup, device work in offline mode");
    skipNetwork = true;
    status(STATUS_OFFLINE_MODE, "Offline mode");
  }

  if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
    enterConfigMode();
  } else {
    enterNormalMode();
  }

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

  uint32_t _millis = millis();
//  if (_millis == lastIterateTime) {
//    return;
//  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->saveIfNeeded();
  }

  checkIfRestartIsNeeded(_millis);
  handleLocalActionTriggers();
  iterateAlwaysElements(_millis);

  if (forceRestartTimeMs) {
    return;
  }

  if (waitForIterate != 0 && _millis - lastIterateTime < waitForIterate) {
    return;
  }

  waitForIterate = 0;
  lastIterateTime = _millis;

  if (skipNetwork) {
    return;
  }

  // Establish and maintain network link
  if (!iterateNetworkSetup()) {
    if (isNetworkSetupOk) {
      Supla::Network::DisconnectProtocols();
    }
    isNetworkSetupOk = false;
    return;
  }
  isNetworkSetupOk = true;

  switch (deviceMode) {
    // Normal and Test mode
    case Supla::DEVICE_MODE_TEST:
    default: {
      // When network is ready iterate over protocol layers
      bool iterateConnected = false;
      for (auto proto = Supla::Protocol::ProtocolLayer::first();
           proto != nullptr;
           proto = proto->next()) {
        if (proto->iterate(_millis)) {
          iterateConnected = true;
        }
        if (proto->isNetworkRestartRequested()) {
          requestNetworkLayerRestart = true;
        }
        delay(0);
      }
      if (iterateConnected) {
        // Iterate all elements
        // Iterate connected exits loop when method returns false, which means
        // that element send message to server. In next iteration we'll start
        // with next element instead of first one on the list.
        if (Supla::Element::IsInvalidPtrSet()) {
          iterateConnectedPtr = nullptr;
          Supla::Element::ClearInvalidPtr();
        }
        if (iterateConnectedPtr == nullptr) {
          iterateConnectedPtr = Supla::Element::begin();
        }
        for (; iterateConnectedPtr != nullptr;
             iterateConnectedPtr = iterateConnectedPtr->next()) {
          if (!iterateConnectedPtr->iterateConnected()) {
            iterateConnectedPtr = iterateConnectedPtr->next();
            break;
          }
          delay(0);
        }
      }

      if (deviceMode == Supla::DEVICE_MODE_TEST) {
      // Test mode
      }
      break;
    }

    // Config mode
    case Supla::DEVICE_MODE_CONFIG: {
      break;
    }

    // SW update mode
    case Supla::DEVICE_MODE_SW_UPDATE: {
      auto cfg = Supla::Storage::ConfigInstance();
      if (swUpdate == nullptr) {
        char url[SUPLA_MAX_URL_LENGTH] = {};
        if (cfg) {
          cfg->getSwUpdateServer(url);
        }
        if (strlen(url) == 0) {
          swUpdate = Supla::Device::SwUpdate::Create(
              this, "https://updates.supla.org/check-updates");
        } else {
          swUpdate = Supla::Device::SwUpdate::Create(this, url);
        }
        if (cfg && swUpdate && cfg->isSwUpdateBeta()) {
          swUpdate->useBeta();
        }
      }
      if (swUpdate == nullptr) {
        SUPLA_LOG_WARNING("Failed to create SW update instance");
      } else if (!swUpdate->isStarted()) {
        status(STATUS_SW_DOWNLOAD, "SW update in progress...");
        SUPLA_LOG_INFO("Starting SW update");
        Supla::Network::DisconnectProtocols();
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
      }
      if (swUpdate == nullptr) {
        deviceMode = Supla::DEVICE_MODE_NORMAL;
        if (cfg) {
          cfg->setDeviceMode(Supla::DEVICE_MODE_NORMAL);
          cfg->setSwUpdateBeta(false);
          cfg->commit();
        }
      }
      break;
    }
  }
}

void SuplaDeviceClass::setServerPort(int value) {
  createSrpcLayerIfNeeded();
  srpcLayer->setServerPort(value);
}

void SuplaDeviceClass::setSwVersion(const char *swVersion) {
  setString(Supla::Channel::reg_dev.SoftVer, swVersion, SUPLA_SOFTVER_MAXSIZE);
}

int SuplaDeviceClass::getCurrentStatus() {
  return currentStatus;
}

void SuplaDeviceClass::fillStateData(TDSC_ChannelState *channelState) {
  channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_UPTIME;

  channelState->Uptime = uptime.getUptime();

  if (!isSleepingDeviceEnabled()) {
    channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_CONNECTIONUPTIME;
    channelState->ConnectionUptime = uptime.getConnectionUptime();
    if (uptime.getLastResetCause() > 0) {
      channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_LASTCONNECTIONRESETCAUSE;
      channelState->LastConnectionResetCause = uptime.getLastResetCause();
    }
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

  // for Supla public servers we check if address is misspelled (srv instead
  // of svr) and replace it
  if (Supla::Protocol::SuplaSrpc::isSuplaPublicServerConfigured()) {
    if (strncmpInsensitive(server, "srv", 3) == 0 && server[3] >= '0' &&
        server[3] <= '9') {
      Supla::Channel::reg_dev.ServerName[1] = 'v';
      Supla::Channel::reg_dev.ServerName[2] = 'r';
    }
  }
}

void SuplaDeviceClass::addClock(Supla::Clock *_clock) {
  (void)(_clock);
  SUPLA_LOG_DEBUG("addClock: DEPRECATED");
}

Supla::Clock *SuplaDeviceClass::getClock() {
  return Supla::Clock::GetInstance();
}

bool SuplaDeviceClass::loadDeviceConfig() {
  auto cfg = Supla::Storage::ConfigInstance();

  bool configComplete = true;
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

  memset(buf, 0, sizeof(buf));
  if (cfg->getAuthKey(buf)) {
    setAuthKey(buf);
  }

  deviceMode = cfg->getDeviceMode();
  if (deviceMode == Supla::DEVICE_MODE_NOT_SET) {
    deviceMode = Supla::DEVICE_MODE_NORMAL;
  } else if (deviceMode == Supla::DEVICE_MODE_TEST) {
    char wifiApName[100] = {};
    const char test[] = "TEST-";
    snprintf(wifiApName, sizeof(wifiApName), "%s", test);
    generateHostname(wifiApName + strlen(test), 0);
    memset(buf, 0, sizeof(buf));
    if (!cfg->getWiFiSSID(buf) || strlen(buf) == 0) {
      cfg->setWiFiSSID(wifiApName);
    } else {
      if (strncmp(wifiApName, buf, strlen(wifiApName)) != 0) {
        SUPLA_LOG_DEBUG(
            "Test mode: leaving. Invalid SSID: %s != %s", wifiApName, buf);
        deviceMode = Supla::DEVICE_MODE_NORMAL;
      }
    }
  }

  // WiFi specific config
  auto net = Supla::Network::Instance();

  if (net != nullptr && net->isWifiConfigRequired()) {
    memset(buf, 0, sizeof(buf));
    if (cfg->getWiFiSSID(buf) && strlen(buf) > 0) {
      net->setSsid(buf);
      configEmpty = false;
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing Wi-Fi SSID");
      addLastStateLog("Missing Wi-Fi SSID");
      configComplete = false;
    }

    memset(buf, 0, sizeof(buf));
    if (cfg->getWiFiPassword(buf) && strlen(buf) > 0) {
      net->setPassword(buf);
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing Wi-Fi password");
      addLastStateLog("Missing Wi-Fi password");
      configComplete = false;
    }
  }

  return configComplete;
}

void SuplaDeviceClass::iterateAlwaysElements(uint32_t _millis) {
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
  if (Supla::Network::PopSetupNeeded()) {
    Supla::Network::Setup();
  }

  if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
    // In config mode we ignore this method
    return true;
  }

  // Restart network after >1 min of failed connection attempts
  if (requestNetworkLayerRestart) {
    requestNetworkLayerRestart = false;
    SUPLA_LOG_WARNING(
        "Network layer restart requested. Trying to setup network "
        "interface again");
    Supla::Network::DisconnectProtocols();
    Supla::Network::Setup();
  }

  if (!Supla::Network::IsReady()) {
    if (networkIsNotReadyCounter == 100) {  // > 10 s
      status(STATUS_NETWORK_DISCONNECTED, "No connection to network");
      uptime.setConnectionLostCause(
          SUPLA_LASTCONNECTIONRESETCAUSE_WIFI_CONNECTION_LOST);
    }
    waitForIterate = 100;  // 0.1 s
    networkIsNotReadyCounter++;
    if (networkIsNotReadyCounter % 100 == 0) {
      SUPLA_LOG_DEBUG("Warning: network is not ready (%d s)",
                      networkIsNotReadyCounter / 10);
      if (networkIsNotReadyCounter % 600 == 0) {
        requestNetworkLayerRestart = true;
      }
    }
    return false;
  } else {
    if (!getSrpcLayer()->isEnabled()) {
      status(STATUS_SUPLA_PROTOCOL_DISABLED, nullptr);
    }
  }

  networkIsNotReadyCounter = 0;
  Supla::Network::Iterate();
  return true;
}

void SuplaDeviceClass::enterConfigMode() {
  if (enterConfigModeTimestamp == 0) {
    enterConfigModeTimestamp = millis();
  }

  skipNetwork = false;

  disableLocalActionsIfNeeded();

  if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
    // if we enter cfg mode with deviceMode already set to cfgmode, then
    // configuration is incomplete, so there is no timeout to leave config
    // mode
    deviceRestartTimeoutTimestamp = 0;
  } else {
    deviceRestartTimeoutTimestamp = millis();
  }

  deviceMode = Supla::DEVICE_MODE_CONFIG;
  Supla::Network::DisconnectProtocols();
  Supla::Network::SetConfigMode();

  if (Supla::Network::PopSetupNeeded()) {
    Supla::Network::Setup();
  }

  if (Supla::WebServer::Instance()) {
    Supla::WebServer::Instance()->start();
  }
  status(STATUS_CONFIG_MODE, "Config mode", true);
}

void SuplaDeviceClass::softRestart() {
  status(STATUS_SOFTWARE_RESET, "Software reset");

  if (!triggerResetToFacotrySettings) {
    // skip writing to storage if reset is because of factory reset
    saveStateToStorage();
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->commit();
    }
  }
  deviceMode = Supla::DEVICE_MODE_NORMAL;

  // TODO(klew): stop supla timers

  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onSoftReset();
    delay(0);
  }

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
}

void SuplaDeviceClass::setManufacturerId(_supla_int16_t id) {
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
    if (request->SuperUserAuthorized != 1) {
      return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
    }
    switch (request->Command) {
      case SUPLA_CALCFG_CMD_ENTER_CFG_MODE: {
        SUPLA_LOG_INFO("CALCFG ENTER CFGMODE received");
        requestCfgMode(Supla::Device::WithTimeout);
        return SUPLA_CALCFG_RESULT_DONE;
      }
      case SUPLA_CALCFG_CMD_SET_TIME: {
        SUPLA_LOG_INFO("CALCFG SET TIME received");
        if (request->DataType != 0 &&
            request->DataSize != sizeof(TSDC_UserLocalTimeResult)) {
          SUPLA_LOG_WARNING("SET TIME invalid size %d", request->DataSize);
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        auto clock = getClock();
        if (clock) {
          clock->parseLocaltimeFromServer(
              reinterpret_cast<TSDC_UserLocalTimeResult *>(request->Data));
          return SUPLA_CALCFG_RESULT_DONE;
        } else {
          return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
        }
      }
      default:
        break;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

void SuplaDeviceClass::saveStateToStorage() {
  if (triggerResetToFacotrySettings) {
    return;
  }

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
    appendixSize = 1 + 2 * macSize;
  }

  char *srcName = customHostnamePrefix;
  if (srcName == nullptr || strlen(srcName) == 0) {
    srcName = Supla::Channel::reg_dev.Name;
  }

  int nameLength = strlen(srcName);

  if (nameLength + appendixSize > 31) {
    nameLength = 31 - appendixSize;
  }

  if (nameLength == 0) {
    setName("SUPLA-DEVICE");
    nameLength = strlen(srcName);
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
      destIdx +=
          generateHexString(mac + (6 - macSize), &(name[destIdx]), macSize);
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
        requestCfgMode(Supla::Device::WithoutTimeout);
      }
      break;
    }
    case Supla::TOGGLE_CONFIG_MODE: {
      if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
        requestCfgMode(Supla::Device::WithoutTimeout);
      } else {
        scheduleSoftRestart(0);
      }
      break;
    }
    case Supla::RESET_TO_FACTORY_SETTINGS: {
      triggerResetToFacotrySettings = true;
      break;
    }
    case Supla::START_LOCAL_WEB_SERVER: {
      triggerStartLocalWebServer = true;
      break;
    }
    case Supla::STOP_LOCAL_WEB_SERVER: {
      triggerStopLocalWebServer = true;
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
        requestCfgMode(Supla::Device::WithoutTimeout);
      } else if (millis() - enterConfigModeTimestamp > 2000) {
        triggerResetToFacotrySettings = true;
      }
      break;
    }
    case Supla::LEAVE_CONFIG_MODE_AND_RESET: {
      if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
        auto cfg = Supla::Storage::ConfigInstance();
        if (allowOfflineMode || (cfg && cfg->isMinimalConfigReady())) {
          scheduleSoftRestart(0);
        }
      }
      break;
    }
  }
}

void SuplaDeviceClass::resetToFactorySettings() {
  // cleanup device's configuration, but keep GUID and AuthKey
  SUPLA_LOG_DEBUG("Reset to factory settings");
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    SUPLA_LOG_DEBUG("Clearing configuration...");
    cfg->removeAll();
    cfg->setGUID(Supla::Channel::reg_dev.GUID);
    cfg->setAuthKey(Supla::Channel::reg_dev.AuthKey);
    cfg->commit();
  }

  // cleanup state storage data
  // TODO(klew): add handling of persistant data (like energy counters)
  auto storage = Supla::Storage::Instance();
  if (storage) {
    SUPLA_LOG_DEBUG("Clearing state storage...");
    storage->deleteAll();
  }
}

void SuplaDeviceClass::handleLocalActionTriggers() {
  if (goToConfigModeAsap != Supla::Device::None) {
    enterConfigMode();
    if (goToConfigModeAsap == Supla::Device::WithoutTimeout) {
      disableCfgModeTimeout();
    }
    goToConfigModeAsap = Supla::Device::None;
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

void SuplaDeviceClass::checkIfRestartIsNeeded(uint32_t _millis) {
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
  if (resetOnConnectionFailTimeoutSec) {
    uint32_t longestConnectionFailTime = networkIsNotReadyCounter / 10;
    for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
         proto = proto->next()) {
      if (proto->getConnectionFailTime() > longestConnectionFailTime) {
        longestConnectionFailTime = proto->getConnectionFailTime();
      }
    }

    if (longestConnectionFailTime >= resetOnConnectionFailTimeoutSec) {
      SUPLA_LOG_INFO("Connection fail timeout %d s - software reset",
                     longestConnectionFailTime);
      softRestart();
    }
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
  resetOnConnectionFailTimeoutSec = timeSec;
}

void SuplaDeviceClass::setLastStateLogger(
    Supla::Device::LastStateLogger *logger) {
  lastStateLogger = logger;
}

void SuplaDeviceClass::setActivityTimeout(_supla_int_t newActivityTimeout) {
  createSrpcLayerIfNeeded();
  srpcLayer->setActivityTimeout(newActivityTimeout);
}

void SuplaDeviceClass::setSuplaCACert(const char *cert) {
  createSrpcLayerIfNeeded();
  srpcLayer->setSuplaCACert(cert);
}

void SuplaDeviceClass::setSupla3rdPartyCACert(const char *cert) {
  createSrpcLayerIfNeeded();
  srpcLayer->setSupla3rdPartyCACert(cert);
}

void SuplaDeviceClass::createSrpcLayerIfNeeded() {
  if (srpcLayer == nullptr) {
    srpcLayer = new Supla::Protocol::SuplaSrpc(this);
  }
}

enum Supla::DeviceMode SuplaDeviceClass::getDeviceMode() {
  return deviceMode;
}

Supla::Protocol::SuplaSrpc *SuplaDeviceClass::getSrpcLayer() {
  return srpcLayer;
}

void SuplaDeviceClass::setCustomHostnamePrefix(const char *prefix) {
  if (prefix == nullptr) {
    if (customHostnamePrefix != nullptr) {
      delete[] customHostnamePrefix;
      customHostnamePrefix = nullptr;
    }
    return;
  }

  int len = strlen(prefix);
  customHostnamePrefix = new char[len + 1];
  strncpy(customHostnamePrefix, prefix, len + 1);
}

void SuplaDeviceClass::disableLocalActionsIfNeeded() {
  // Disable local actions/buttons if minimal config is ready.
  // This is required to have buttons working for device with empty
  // configuration, instead of handling device reset
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && cfg->isMinimalConfigReady()) {
    auto ptr = Supla::ActionHandlerClient::begin;
    while (ptr) {
      if (ptr->trigger && ptr->trigger->disableActionsInConfigMode()) {
        ptr->disable();  // some actions can be created with "alwaysEnabled"
                         // flag in such case, disable() has no effect
      }
      ptr = ptr->next;
    }
  }
}

void SuplaDeviceClass::disableNetwork() {
  skipNetwork = true;
  Supla::Network::Disable();
}

void SuplaDeviceClass::enableNetwork() {
  skipNetwork = false;
  Supla::Network::SetSetupNeeded();
}

void SuplaDeviceClass::requestCfgMode(
    Supla::Device::RequestConfigModeType type) {
  goToConfigModeAsap = type;
}

bool SuplaDeviceClass::isSleepingDeviceEnabled() {
  return (Supla::Channel::reg_dev.Flags &
          SUPLA_DEVICE_FLAG_SLEEP_MODE_ENABLED) != 0;
}

uint32_t SuplaDeviceClass::getActivityTimeout() {
  createSrpcLayerIfNeeded();
  return srpcLayer->getActivityTimeout();
}

bool SuplaDeviceClass::getStorageInitResult() {
  return storageInitResult;
}

// Sleeping is allowed only in normal and test mode.
// Additionally sleeping is not allowed, when device restet is requested.
bool SuplaDeviceClass::isSleepingAllowed() {
  return (getDeviceMode() == Supla::DEVICE_MODE_NORMAL ||
          getDeviceMode() == Supla::DEVICE_MODE_TEST) &&
         forceRestartTimeMs == 0;
}

void SuplaDeviceClass::allowWorkInOfflineMode(int mode) {
  allowOfflineMode = mode;
}

SuplaDeviceClass SuplaDevice;
