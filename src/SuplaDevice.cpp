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
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/timer.h>
#include <supla/tools.h>
#include <supla/version.h>
#include <supla/device/register_device.h>
#include <supla/mutex.h>
#include <supla/auto_lock.h>
#include <supla/device/subdevice_pairing_handler.h>
#include <supla/device/status_led.h>
#include <supla/clock/clock.h>
#include <supla/device/remote_device_config.h>
#include <supla/device/auto_update_policy.h>
#include <supla/device/device_mode.h>
#include <supla/device/security_logger.h>
#include <supla/device/factory_test.h>

#ifndef ARDUINO
#ifndef F
#define F(arg_f) arg_f
#endif
#endif

void SuplaDeviceClass::status(int newStatus,
                              const __FlashStringHelper *msg,
                              bool alwaysLog) {
  bool showLog = false;

#ifdef ARDUINO
  String msgStr(msg);
  const char *msgActual = msgStr.c_str();
#else
  const char *msgActual = msg;
#endif

  if (currentStatus == STATUS_TEST_WAIT_FOR_CFG_BUTTON &&
      newStatus != STATUS_SOFTWARE_RESET) {
    // testing mode is final state and the only exit goes through
    // reset with exception for: software reset
    return;
  }

  if (currentStatus != newStatus) {
    if (!(newStatus == STATUS_REGISTER_IN_PROGRESS &&
          currentStatus > STATUS_REGISTER_IN_PROGRESS)) {
      if (impl_arduino_status != nullptr) {
        impl_arduino_status(newStatus, msgActual);
      }
      currentStatus = newStatus;
      showLog = true;
      if (newStatus != STATUS_INITIALIZED && msgActual != nullptr) {
        addLastStateLog(msgActual);
      }
      runAction(Supla::ON_DEVICE_STATUS_CHANGE);
    }
  }
  if ((alwaysLog || showLog) && msgActual != nullptr) {
    SUPLA_LOG_INFO("Current status: [%d] %s", newStatus, msgActual);
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
  timerAccessMutex = Supla::Mutex::Create();
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
  if (timerAccessMutex) {
    delete timerAccessMutex;
    timerAccessMutex = nullptr;
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
    status(STATUS_ALREADY_INITIALIZED, F("SuplaDevice is already initialized"));
    return false;
  }
  initializationDone = true;

  SUPLA_LOG_INFO(" *** Supla - starting initialization (platform %d)",
                  Supla::getPlatformId());

  if (getClock() == nullptr) {
    SUPLA_LOG_DEBUG("Clock not configured, using default clock");
    new Supla::Clock();
  }

  if (protoVersion < 23) {
    SUPLA_LOG_ERROR(
        "Minimum supported protocol version is 23! Setting to 23 anyway");
    protoVersion = 23;
  }
  if (protoVersion >= 21) {
    SUPLA_LOG_DEBUG("SD: add flag DEVICE_CONFIG_SUPPORTED");
    addFlags(SUPLA_DEVICE_FLAG_DEVICE_CONFIG_SUPPORTED);
  }
  if (isDeviceSoftwareResetSupported()) {
    SUPLA_LOG_DEBUG("SD: add flag CALCFG_RESTART_DEVICE");
    addFlags(SUPLA_DEVICE_FLAG_CALCFG_RESTART_DEVICE);
  }

  // Initialize protocol layers
  createSrpcLayerIfNeeded();
  srpcLayer->setVersion(protoVersion);

  storageInitResult = Supla::Storage::Init();

  if (Supla::Storage::IsConfigStorageAvailable()) {
    SUPLA_LOG_INFO(" *** Supla - Config initalization");

    if (!lastStateLogger) {
      lastStateLogger = new Supla::Device::LastStateLogger();
    }

    if (Supla::Storage::ConfigInstance()->isConfigModeSupported()) {
      SUPLA_LOG_DEBUG("SD: add flag CALCFG_ENTER_CFG_MODE");
      addFlags(SUPLA_DEVICE_FLAG_CALCFG_ENTER_CFG_MODE);
      SUPLA_LOG_DEBUG("SD: add flag CALCFG_FACTORY_RESET_SUPPORTED");
      addFlags(SUPLA_DEVICE_FLAG_CALCFG_FACTORY_RESET_SUPPORTED);
    }

    // Load device and network related configuration
    if (!loadDeviceConfig()) {
      configurationState.configNotComplete = 1;
    }

    // Load protocol layer configuration
    for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
         proto = proto->next()) {
      if (!proto->onLoadConfig()) {
        if (deviceMode != Supla::DEVICE_MODE_TEST) {
          configurationState.configNotComplete = 1;
        }
      }
      if (!proto->isConfigEmpty()) {
        configurationState.protocolNotEmpty = 1;
      }
      if (proto->isEnabled()) {
        configurationState.atLeastOneProtoIsEnabled = 1;
      }
      delay(0);
    }

    if (!configurationState.atLeastOneProtoIsEnabled) {
      status(STATUS_ALL_PROTOCOLS_DISABLED,
             F("All communication protocols are disabled"));
      if (deviceMode != Supla::DEVICE_MODE_TEST) {
        configurationState.configNotComplete = 1;
      }
    }

    SUPLA_LOG_INFO(" *** Supla - Config load for elements");
    // Load elements configuration
    for (auto element = Supla::Element::begin(); element != nullptr;
         element = element->next()) {
      element->onLoadConfig(this);
      delay(0);
    }
    SUPLA_LOG_INFO(" *** Supla - Config load for elements done");
  }

  if (Supla::Storage::Instance()) {
    SUPLA_LOG_INFO(" *** Supla - Load state storage");
    // Pefrorm dry run of write state to validate stored state section with
    // current device configuration
    if (Supla::Storage::IsStateStorageValid()) {
      Supla::Storage::LoadStateStorage();
    }
    SUPLA_LOG_INFO(" *** Supla - Load state storage done");
  }

  SUPLA_LOG_INFO(" *** Supla - Init elements");
  // Initialize elements
  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onInit();
    delay(0);
  }
  SUPLA_LOG_INFO(" *** Supla - Init elements done");

  // Enable timers
  Supla::initTimers();

  if (Supla::Network::Instance() == nullptr) {
    status(STATUS_MISSING_NETWORK_INTERFACE,
           F("Network Interface not defined!"));
    return false;
  }

  for (auto net = Supla::Network::FirstInstance(); net != nullptr;
       net = Supla::Network::NextInstance(net)) {
    net->setSuplaDeviceClass(this);
  }

  if (auto webServer = Supla::WebServer::Instance()) {
    webServer->setSuplaDeviceClass(this);
    if (webServer->verifyCertificatesFormat()) {
      // web password is used only when https is used
      SUPLA_LOG_DEBUG(
          "SD: add flag CALCFG_SET_CFG_MODE_PASSWORD_SUPPORTED");
      addFlags(SUPLA_DEVICE_FLAG_CALCFG_SET_CFG_MODE_PASSWORD_SUPPORTED);
    }
  }

  if (Supla::RegisterDevice::isGUIDEmpty()) {
    status(STATUS_INVALID_GUID, F("Missing GUID"));
    return false;
  }

  if (Supla::RegisterDevice::isAuthKeyEmpty()) {
    status(STATUS_INVALID_AUTHKEY, F("Missing AuthKey"));
    return false;
  }

  if (configurationState.configNotComplete) {
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

  char buf[SUPLA_GUID_SIZE * 2 + 1] = {};
  generateHexString(Supla::RegisterDevice::getGUID(), buf, SUPLA_GUID_SIZE);
  SUPLA_LOG_INFO("GUID: %s", buf);

  if (Supla::RegisterDevice::isNameEmpty()) {
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

  if (Supla::RegisterDevice::isSoftVerEmpty()) {
    setSwVersion(suplaDeviceVersion);
  }
  SUPLA_LOG_INFO("Device name: %s", Supla::RegisterDevice::getName());
  SUPLA_LOG_INFO("Device software version: %s",
      Supla::RegisterDevice::getSoftVer());

  SUPLA_LOG_INFO(" *** Supla - Initializing network layer");
  char hostname[32] = {};
  generateHostname(hostname, macLengthInHostname);
  Supla::Network::SetHostname(hostname, macLengthInHostname);

  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
       proto = proto->next()) {
    proto->onInit();
    delay(0);
  }

  status(STATUS_INITIALIZED, F("SuplaDevice initialized"));

  setupDeviceMode();

  SUPLA_LOG_INFO(" *** Supla - Initialization done");
  if (deviceMode != Supla::DEVICE_MODE_TEST) {
    SUPLA_LOG_INFO(" *** Self-test ***");
    auto tester = new Supla::Device::FactoryTest(this, 0);
    tester->onInit();
    delete tester;
    SUPLA_LOG_INFO(" *** Self-test done ***");
  }
  return true;
}

void SuplaDeviceClass::setupDeviceMode() {
  // Setup device mode is called at the end of begin() method and
  // when device leaves config mode without restart.

  if (configurationState.configNotComplete) {
    SUPLA_LOG_INFO("Config incomplete: deviceMode = CONFIG");
    deviceMode = Supla::DEVICE_MODE_CONFIG;
  } else if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
    deviceMode = Supla::DEVICE_MODE_NORMAL;
  }

  switch (initialMode) {
    case Supla::InitialMode::StartOffline: {
      cfgModeState = Supla::CfgModeState::Done;
      if (deviceMode == Supla::DEVICE_MODE_CONFIG &&
          configurationState.isEmpty()) {
        deviceMode = Supla::DEVICE_MODE_OFFLINE;
      }
      break;
    }

    case Supla::InitialMode::StartInNotConfiguredMode: {
      cfgModeState = Supla::CfgModeState::Done;
      if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
        deviceMode = Supla::DEVICE_MODE_NOT_CONFIGURED;
      }
      break;
    }

    case Supla::InitialMode::StartInCfgMode: {
      cfgModeState = Supla::CfgModeState::Done;
      break;
    }

    case Supla::InitialMode::StartWithCfgModeThenOffline: {
      if (deviceMode == Supla::DEVICE_MODE_CONFIG &&
          (!configurationState.atLeastOneProtoIsEnabled ||
           cfgModeState == Supla::CfgModeState::Done)) {
        deviceMode = Supla::DEVICE_MODE_OFFLINE;
      }
      if (cfgModeState == Supla::CfgModeState::NotSet) {
        cfgModeState = Supla::CfgModeState::CfgModeStartedFor1hPending;
      }
      break;
    }
  }

  switch (deviceMode) {
    case Supla::DEVICE_MODE_OFFLINE: {
      SUPLA_LOG_INFO("Disabling network setup, device work in offline mode");
      skipNetwork = true;
      status(STATUS_OFFLINE_MODE, F("Offline mode"));
      Supla::Network::SetOfflineMode();
      return;
    }
    case Supla::DEVICE_MODE_NOT_CONFIGURED: {
      SUPLA_LOG_INFO(
          "Disabling network setup, device work in not configured mode");
      skipNetwork = true;
      status(STATUS_NOT_CONFIGURED_MODE, F("Not configured mode"));
      Supla::Network::SetOfflineMode();
      return;
    }
    case Supla::DEVICE_MODE_CONFIG: {
      if (configurationState.isEmpty()) {
        // For empty configuration we don't want to show all missing fields,
        // so we clear logger
        lastStateLogger->clear();
      }
      addSecurityLog(Supla::SecurityLogSource::LOCAL_DEVICE,
                     "Device in config mode");
      enterConfigMode();
      break;
    }
    default: {
      enterNormalMode();
      break;
    }
  }
}

void SuplaDeviceClass::setName(const char *name) {
  Supla::RegisterDevice::setName(name);
}

void SuplaDeviceClass::onTimer(void) {
  Supla::AutoLock lock(timerAccessMutex);
  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onTimer();
  }
}

void SuplaDeviceClass::onFastTimer(void) {
  Supla::AutoLock lock(timerAccessMutex);
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

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->saveIfNeeded();
  }

  checkIfLeaveCfgModeOrRestartIsNeeded();
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
    case Supla::DEVICE_MODE_NORMAL:
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
        // check SW update availability
        if (swUpdate == nullptr && isAutomaticFirmwareUpdateEnabled()) {
          if (millis() - lastSwUpdateCheckTimestamp >
              SUPLA_AUTOMATIC_OTA_CHECK_INTERVAL) {
            initSwUpdateInstance(false, -1);
            lastSwUpdateCheckTimestamp = millis();
            if (swUpdate) {
              triggerSwUpdateIfAvailable = true;
            }
          }
        }
        iterateSwUpdate();
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
      if (swUpdate == nullptr) {
        initSwUpdateInstance(true, 0);
      }
      iterateSwUpdate();
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

bool SuplaDeviceClass::initSwUpdateInstance(bool performUpdate,
                                            int securityOnly) {
  if (swUpdate) {
    // already initialized earlier
    return true;
  }
  if (!isAutomaticFirmwareUpdateEnabled() && securityOnly != 0) {
    return false;
  }

  lastSwUpdateCheckTimestamp = millis();

  char url[SUPLA_MAX_URL_LENGTH] = {};
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    if (securityOnly == -1) {
      switch (cfg->getAutoUpdatePolicy()) {
        default:
        case Supla::AutoUpdatePolicy::ForcedOff: {
          SUPLA_LOG_INFO("Firmware update is forced off");
          return false;
        }
        case Supla::AutoUpdatePolicy::Disabled: {
          SUPLA_LOG_INFO("Firmware update is disabled");
          return false;
        }
        case Supla::AutoUpdatePolicy::SecurityOnly: {
          SUPLA_LOG_INFO("Firmware update is security only");
          securityOnly = 1;
          break;
        }
        case Supla::AutoUpdatePolicy::AllUpdates: {
          SUPLA_LOG_INFO("Firmware update is enabled for all updates");
          securityOnly = 0;
          break;
        }
      }
    }
    cfg->getSwUpdateServer(url);
  }

  if (strlen(url) == 0) {
    swUpdate = Supla::Device::SwUpdate::Create(
        this, "https://iot.updates.supla.org/check-updates");
  } else {
    swUpdate = Supla::Device::SwUpdate::Create(this, url);
  }
  if (swUpdate == nullptr) {
    SUPLA_LOG_WARNING("Failed to create SW update instance");
    return false;
  }

  if (cfg) {
    if (cfg->isSwUpdateBeta()) {
      swUpdate->useBeta();
    }
    if (cfg->isSwUpdateSkipCert()) {
      swUpdate->setSkipCert();
    }
  }
  if (securityOnly == 1) {
    swUpdate->setSecurityOnly();
  }
  if (!performUpdate) {
    SUPLA_LOG_INFO("Checking SW update");
    swUpdate->setCheckUpdateAndAbort();
  } else {
    deviceMode = Supla::DEVICE_MODE_SW_UPDATE;
  }
  return swUpdate != nullptr;
}

void SuplaDeviceClass::iterateSwUpdate() {
  if (swUpdate == nullptr) {
    return;
  }

  if (!swUpdate->isStarted()) {
    if (!swUpdate->isCheckUpdateAndAbort()) {
      SUPLA_LOG_INFO("Starting SW update");
      status(STATUS_SW_DOWNLOAD, F("SW update in progress..."));
      Supla::Network::DisconnectProtocols();
    }
    swUpdate->start();
  } else {
    swUpdate->iterate();
    if (swUpdate->isAborted()) {
      SUPLA_LOG_INFO("SW update aborted");
      int securityOnly = swUpdate->isSecurityOnly() ? 1 : 0;
      TCalCfg_FirmwareCheckResult result = {};
      result.Result = SUPLA_FIRMWARE_CHECK_RESULT_ERROR;
      if (swUpdate->getNewVersion()) {
        SUPLA_LOG_INFO("New version available: %s", swUpdate->getNewVersion());
        strncpy(result.SoftVer,
                swUpdate->getNewVersion(),
                SUPLA_SOFTVER_MAXSIZE - 1);
        result.Result = SUPLA_FIRMWARE_CHECK_RESULT_UPDATE_AVAILABLE;
        if (swUpdate->getChangelogUrl()) {
          strncpy(
              result.ChangelogUrl, swUpdate->getChangelogUrl(),
              SUPLA_URL_PATH_MAXSIZE - 1);
        }
      } else {
        result.Result = SUPLA_FIRMWARE_CHECK_RESULT_UPDATE_NOT_AVAILABLE;
        triggerSwUpdateIfAvailable = false;
      }
      srpcLayer->sendPendingCalCfgResult(-1, SUPLA_CALCFG_RESULT_TRUE, -1,
          sizeof(result), &result);
      srpcLayer->clearPendingCalCfgResult(
          -1, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
      delete swUpdate;
      swUpdate = nullptr;
      if (triggerSwUpdateIfAvailable) {
        initSwUpdateInstance(true, securityOnly);
      }

    } else if (swUpdate->isFinished()) {
      SUPLA_LOG_INFO("Finished SW update, restarting...");
      delete swUpdate;
      swUpdate = nullptr;
      scheduleSoftRestart();
    }
  }
}

void SuplaDeviceClass::setServerPort(int value) {
  createSrpcLayerIfNeeded();
  srpcLayer->setServerPort(value);
}

void SuplaDeviceClass::setSwVersion(const char *swVersion) {
  Supla::RegisterDevice::setSoftVer(swVersion);
}

int8_t SuplaDeviceClass::getCurrentStatus() const {
  return currentStatus;
}

void SuplaDeviceClass::fillStateData(TDSC_ChannelState *channelState) const {
  if (showUptimeInChannelState) {
    channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_UPTIME;
    channelState->Uptime = uptime.getUptime();
  }

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
  Supla::RegisterDevice::setGUID(GUID);
}

void SuplaDeviceClass::setAuthKey(const char authkey[SUPLA_AUTHKEY_SIZE]) {
  Supla::RegisterDevice::setAuthKey(authkey);
}

void SuplaDeviceClass::setEmail(const char *email) {
  Supla::RegisterDevice::setEmail(email);
}

void SuplaDeviceClass::setServer(const char *server) {
  Supla::RegisterDevice::setServerName(server);
}

void SuplaDeviceClass::addClock(Supla::Clock *) {
  SUPLA_LOG_DEBUG("addClock: DEPRECATED");
}

Supla::Clock *SuplaDeviceClass::getClock() const {
  return Supla::Clock::GetInstance();
}

bool SuplaDeviceClass::loadDeviceConfig() {
  auto cfg = Supla::Storage::ConfigInstance();

  bool configComplete = true;
  char buf[512] = {};

  // Device generic config
  memset(buf, 0, sizeof(buf));
  if (cfg->getDeviceName(buf)) {
    setName(buf);
  }

  bool generateGuidAndAuthkey = false;
  memset(buf, 0, sizeof(buf));
  if (cfg->getGUID(buf)) {
    setGUID(buf);
  } else {
    generateGuidAndAuthkey = true;
  }

  memset(buf, 0, sizeof(buf));
  if (cfg->getAuthKey(buf)) {
    setAuthKey(buf);
  } else {
    generateGuidAndAuthkey = true;
  }

  if (generateGuidAndAuthkey) {
    if (cfg->generateGuidAndAuthkey()) {
      SUPLA_LOG_INFO("Successfully generated GUID and AuthKey");
      if (cfg->getGUID(buf)) {
        setGUID(buf);
      }
      if (cfg->getAuthKey(buf)) {
        setAuthKey(buf);
      }
      generateHexString(
          Supla::RegisterDevice::getAuthKey(), buf, SUPLA_AUTHKEY_SIZE);
      SUPLA_LOG_DEBUG("New AuthKey: %s", buf);
      cfg->initDefaultDeviceConfig();
    } else {
      SUPLA_LOG_ERROR("Failed to generate GUID and AuthKey");
      return true;
    }
  }

  if (!isAutomaticFirmwareUpdateEnabled()) {
    SUPLA_LOG_WARNING("Automatic firmware update is disabled");
  } else {
    auto otaPolicy = cfg->getAutoUpdatePolicy();
    (void)(otaPolicy);
    SUPLA_LOG_INFO(
        "Automatic firmware update is supported. OTA policy: %s",
        otaPolicy == Supla::AutoUpdatePolicy::ForcedOff      ? "forced off"
        : otaPolicy == Supla::AutoUpdatePolicy::Disabled     ? "disabled"
        : otaPolicy == Supla::AutoUpdatePolicy::SecurityOnly ? "security only"
                                                             : "all enabled");
  }

  deviceMode = cfg->getDeviceMode();
  SUPLA_LOG_INFO("Device mode: %d", deviceMode);
  if (deviceMode == Supla::DEVICE_MODE_NOT_SET) {
    deviceMode = Supla::DEVICE_MODE_NORMAL;
  } else if (deviceMode == Supla::DEVICE_MODE_TEST) {
    Supla::Network::SetTestMode();
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
  Supla::Network::LoadConfig();

  if (net != nullptr && !net->isIntfDisabledInConfig() &&
      net->isWifiConfigRequired()) {
    memset(buf, 0, sizeof(buf));
    configurationState.wifiEnabled = 1;
    if (cfg->getWiFiSSID(buf) && strlen(buf) > 0) {
      net->setSsid(buf);
      configurationState.wifiSsidFilled = 1;
    } else {
      SUPLA_LOG_INFO("Config incomplete: missing Wi-Fi SSID");
      addLastStateLog("Missing Wi-Fi SSID");
      configComplete = false;
    }

    memset(buf, 0, sizeof(buf));
    if (cfg->getWiFiPassword(buf) && strlen(buf) > 0) {
      net->setPassword(buf);
      configurationState.wifiPassFilled = 1;
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
  if (requestNetworkLayerRestart || Supla::Network::IsIpSetupTimeout()) {
    requestNetworkLayerRestart = false;
    SUPLA_LOG_WARNING(
        "Network layer restart requested. Trying to setup network "
        "interface again");
    Supla::Network::DisconnectProtocols();
    Supla::Network::Setup();
  }

  if (!Supla::Network::IsReady()) {
    if (networkIsNotReadyCounter == 200) {  // > 20 s
      status(STATUS_NETWORK_DISCONNECTED, F("No connection to network"));
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

  deviceMode = Supla::DEVICE_MODE_CONFIG;
  Supla::Network::DisconnectProtocols();
  Supla::Network::SetConfigMode();

  if (isLeaveCfgModeAfterInactivityEnabled()) {
    cfgModeState = Supla::CfgModeState::CfgModeStartedPending;
  }

  if (Supla::Network::PopSetupNeeded()) {
    Supla::Network::Setup();
  }

  if (Supla::WebServer::Instance()) {
    Supla::WebServer::Instance()->start();
  }
  status(STATUS_CONFIG_MODE, F("Config mode"), true);
}

void SuplaDeviceClass::leaveConfigModeWithoutRestart() {
  cfgModeState = Supla::CfgModeState::Done;
  if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
    return;
  }
  addSecurityLog(Supla::SecurityLogSource::LOCAL_DEVICE, "Leaving config mode");
  enterConfigModeTimestamp = 0;

  if (Supla::WebServer::Instance()) {
    Supla::WebServer::Instance()->stop();
  }

  setupDeviceMode();

  if (Supla::Network::PopSetupNeeded()) {
    Supla::Network::Setup();
  }
}

void SuplaDeviceClass::softRestart() {
  status(STATUS_SOFTWARE_RESET, F("Software reset"));
  if (statusLed) {
    statusLed->setAlwaysOffSequence();
  }

  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onSoftReset();
    delay(0);
  }

  if (!triggerResetToFactorySettings) {
    // skip writing to storage if reset is because of factory reset
    saveStateToStorage();
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->commit();
    }
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
}

void SuplaDeviceClass::setManufacturerId(int16_t id) {
  Supla::RegisterDevice::setManufacturerId(id);
}

void SuplaDeviceClass::setProductId(int16_t id) {
  Supla::RegisterDevice::setProductId(id);
}

void SuplaDeviceClass::addFlags(int32_t newFlags) {
  Supla::RegisterDevice::addFlags(newFlags);
}

void SuplaDeviceClass::removeFlags(int32_t flags) {
  Supla::RegisterDevice::removeFlags(flags);
}

int SuplaDeviceClass::handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request,
                                             TDS_DeviceCalCfgResult *result) {
  if (request) {
    if (request->SuperUserAuthorized != 1) {
      return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
    }
    switch (request->Command) {
      case SUPLA_CALCFG_CMD_ENTER_CFG_MODE: {
        SUPLA_LOG_INFO("CALCFG ENTER CFGMODE received");
        addSecurityLog(Supla::SecurityLogSource::REMOTE,
                       "Enter config mode from remote request");
        requestCfgMode();
        cfgModeState = Supla::CfgModeState::CfgModeStartedPending;
        return SUPLA_CALCFG_RESULT_DONE;
      }
      case SUPLA_CALCFG_CMD_RESTART_DEVICE: {
        SUPLA_LOG_INFO("CALCFG RESTART DEVICE received");
        scheduleSoftRestart(1);
        return SUPLA_CALCFG_RESULT_DONE;
      }
      case SUPLA_CALCFG_CMD_SET_TIME: {
        SUPLA_LOG_INFO("CALCFG SET TIME received");
        if (request->DataType != 0 ||
            request->DataSize > sizeof(TSDC_UserLocalTimeResult)) {
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
      case SUPLA_CALCFG_CMD_START_SUBDEVICE_PAIRING: {
        SUPLA_LOG_INFO("CALCFG START SUBDEVICE PAIRING received");
        if (subdevicePairingHandler &&
            Supla::RegisterDevice::isPairingSubdeviceEnabled()) {
          TCalCfg_SubdevicePairingResult pairingResult = {};
          subdevicePairingHandler->startPairing(getSrpcLayer(), &pairingResult);
          if (result && sizeof(pairingResult) <= SUPLA_CALCFG_DATA_MAXSIZE) {
            memcpy(result->Data, &pairingResult, sizeof(pairingResult));
            result->DataSize = sizeof(pairingResult);
          } else {
            SUPLA_LOG_WARNING(
                "No result buffer provided, or size is too big %d > %d",
                sizeof(pairingResult),
                SUPLA_CALCFG_DATA_MAXSIZE);
            return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
          }
          return SUPLA_CALCFG_RESULT_TRUE;
        } else {
          SUPLA_LOG_WARNING("No SubdevicePairingHandler configured");
          return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
        }
      }
      case SUPLA_CALCFG_CMD_IDENTIFY_DEVICE: {
        SUPLA_LOG_INFO("CALCFG IDENTIFY DEVICE received");
        identifyStatusLed();
        return SUPLA_CALCFG_RESULT_DONE;
      }
      case SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE: {
        SUPLA_LOG_INFO("CALCFG CHECK FIRMWARE UPDATE received");
        if (!isAutomaticFirmwareUpdateEnabled()) {
          SUPLA_LOG_WARNING("Automatic firmware update not supported");
          return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
        }
        if (swUpdate) {
          // update ongoing
          SUPLA_LOG_INFO("Firmware update in progress");
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        auto cfg = Supla::Storage::ConfigInstance();
        auto otaMode = Supla::AutoUpdatePolicy::ForcedOff;
        if (cfg) {
          otaMode = cfg->getAutoUpdatePolicy();
        }
        switch (otaMode) {
          case Supla::AutoUpdatePolicy::ForcedOff: {
            SUPLA_LOG_INFO("Firmware update is forced off");
            return SUPLA_CALCFG_RESULT_FALSE;
          }
          case Supla::AutoUpdatePolicy::Disabled:
          case Supla::AutoUpdatePolicy::SecurityOnly:
          case Supla::AutoUpdatePolicy::AllUpdates: {
            break;
          }
        }
        // only check firmware for all updates
        initSwUpdateInstance(false, 0);
        if (swUpdate) {
          SUPLA_LOG_INFO("Firmware update check started");
          return SUPLA_CALCFG_RESULT_TRUE;
        }
        SUPLA_LOG_WARNING("Failed to create firmware update instance");
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      case SUPLA_CALCFG_CMD_START_FIRMWARE_UPDATE: {
        SUPLA_LOG_INFO("CALCFG START FIRMWARE UPDATE received");
        if (!isAutomaticFirmwareUpdateEnabled()) {
          SUPLA_LOG_WARNING("Automatic firmware update not supported");
          return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
        }
        if (swUpdate) {
          // update ongoing
          SUPLA_LOG_INFO("Firmware update in progress");
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        auto cfg = Supla::Storage::ConfigInstance();
        auto otaPolicy = Supla::AutoUpdatePolicy::ForcedOff;
        if (cfg) {
          otaPolicy = cfg->getAutoUpdatePolicy();
        }
        switch (otaPolicy) {
          case Supla::AutoUpdatePolicy::ForcedOff: {
            SUPLA_LOG_INFO("Firmware update is forced off");
            return SUPLA_CALCFG_RESULT_FALSE;
          }
          case Supla::AutoUpdatePolicy::Disabled:
          case Supla::AutoUpdatePolicy::SecurityOnly:
          case Supla::AutoUpdatePolicy::AllUpdates: {
            break;
          }
        }
        // only check firmware for all updates
        initSwUpdateInstance(false, 0);
        triggerSwUpdateIfAvailable = true;
        if (swUpdate) {
          SUPLA_LOG_INFO("Firmware update started");
          return SUPLA_CALCFG_RESULT_TRUE;
        }
        SUPLA_LOG_WARNING("Failed to create firmware update instance");
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      case SUPLA_CALCFG_CMD_START_SECURITY_UPDATE: {
        SUPLA_LOG_INFO("CALCFG START SECURITY UPDATE received");
        if (!isAutomaticFirmwareUpdateEnabled()) {
          SUPLA_LOG_WARNING("Automatic firmware update not supported");
          return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
        }
        if (swUpdate) {
          // update ongoing
          SUPLA_LOG_INFO("Firmware update in progress");
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        auto cfg = Supla::Storage::ConfigInstance();
        auto otaMode = Supla::AutoUpdatePolicy::ForcedOff;
        if (cfg) {
          otaMode = cfg->getAutoUpdatePolicy();
        }
        switch (otaMode) {
          case Supla::AutoUpdatePolicy::ForcedOff: {
            SUPLA_LOG_INFO("Firmware update is forced off");
            return SUPLA_CALCFG_RESULT_FALSE;
          }
          case Supla::AutoUpdatePolicy::Disabled:
          case Supla::AutoUpdatePolicy::SecurityOnly:
          case Supla::AutoUpdatePolicy::AllUpdates: {
            break;
          }
        }
        // only check firmware for all updates
        initSwUpdateInstance(false, 1);
        triggerSwUpdateIfAvailable = true;
        if (swUpdate) {
          SUPLA_LOG_INFO("Firmware update started");
          return SUPLA_CALCFG_RESULT_TRUE;
        }
        SUPLA_LOG_WARNING("Failed to create firmware update instance");
        return SUPLA_CALCFG_RESULT_FALSE;
      }

      case SUPLA_CALCFG_CMD_RESET_TO_FACTORY_SETTINGS: {
        SUPLA_LOG_INFO("CALCFG RESET TO FACTORY SETTINGS received");
        triggerResetToFactorySettings = true;
        return SUPLA_CALCFG_RESULT_DONE;
      }

      case SUPLA_CALCFG_CMD_SET_CFG_MODE_PASSWORD: {
        SUPLA_LOG_INFO("CALCFG SET CFGMODE PASSWORD received");
        if (request->DataType != 0 ||
            request->DataSize > sizeof(TCalCfg_SetCfgModePassword)) {
          SUPLA_LOG_WARNING("CALCFG SET CFGMODE PASSWORD invalid size %d",
                            request->DataSize);
          return SUPLA_CALCFG_RESULT_FALSE;
        }

        auto cfg = Supla::Storage::ConfigInstance();
        if (!cfg || !Supla::RegisterDevice::isSetCfgModePasswordEnabled()) {
          return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
        }

        TCalCfg_SetCfgModePassword *password =
            reinterpret_cast<TCalCfg_SetCfgModePassword *>(request->Data);

        Supla::SaltPassword saltPassword;
        if (!saltPassword.isPasswordStrong(password->NewPassword)) {
          SUPLA_LOG_WARNING("CALCFG SET CFGMODE PASSWORD: password too weak");
          addSecurityLog(
              Supla::SecurityLogSource::REMOTE,
              "Password change failed: password is not strong enough");
          return SUPLA_CALCFG_RESULT_FALSE;
        }
        Supla::Config::generateSaltPassword(password->NewPassword,
                                            &saltPassword);
        cfg->setCfgModeSaltPassword(saltPassword);
        addSecurityLog(Supla::SecurityLogSource::REMOTE,
                       "Password successfully changed");
        cfg->saveWithDelay(2000);
        SUPLA_LOG_INFO("CALCFG SET CFGMODE PASSWORD: new password set");
        return SUPLA_CALCFG_RESULT_DONE;
      }

      default:
        break;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

void SuplaDeviceClass::saveStateToStorage() {
  if (triggerResetToFactorySettings) {
    return;
  }
  Supla::Storage::WriteStateStorage();
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

  const char *srcName = customHostnamePrefix;
  if (srcName == nullptr || strlen(srcName) == 0) {
    srcName = Supla::RegisterDevice::getName();
  }

  int srcNameLength = strlen(srcName);
  int targetNameLength = srcNameLength;

  int destIdx = 0;
  int i = 0;

  if (strncmp(srcName, "OH!", 3) == 0) {
    strncpy(name, "SUPLA-", 7);
    destIdx = 6;
    i = 3;
    targetNameLength += 3;
  }

  int skipBytes = 0;

  if (targetNameLength + appendixSize > 31) {
    skipBytes = (targetNameLength + appendixSize) - 31;
  }

  if (srcNameLength == 0) {
    setName("SUPLA-DEVICE");
    srcNameLength = strlen(srcName);
  }

  for (; i < srcNameLength - skipBytes; i++) {
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
    } else {
      continue;
    }
    if (destIdx == 1) {
      if (name[0] == '-') {
        if (skipBytes) {
          skipBytes--;
        }
        destIdx--;
      }
    } else if (name[destIdx - 2] == '-' && name[destIdx - 1] == '-') {
      if (skipBytes) {
        skipBytes--;
      }
      destIdx--;
    }
  }

  name[destIdx++] = 0;
  strncpy(buf, name, 32);
  buf[31] = 0;

  return destIdx;
}

void SuplaDeviceClass::restartCfgModeTimeout(bool requireRestart) {
  if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
    return;
  }

  if (forceRestartTimeMs == 0) {
    if (requireRestart || deviceRestartTimeoutTimestamp) {
      cfgModeState = Supla::CfgModeState::Done;
      deviceRestartTimeoutTimestamp = millis();
    }
    enterConfigModeTimestamp = millis();
    runAction(Supla::ON_DEVICE_STATUS_CHANGE);
  }
}

void SuplaDeviceClass::scheduleSoftRestart(int timeout) {
  SUPLA_LOG_INFO("Scheduling soft restart in %d ms", timeout);
  status(STATUS_SOFTWARE_RESET, F("Software reset"));
  if (timeout <= 0) {
    forceRestartTimeMs = 1;
  } else {
    forceRestartTimeMs = timeout;
  }
  cfgModeState = Supla::CfgModeState::Done;
  deviceRestartTimeoutTimestamp = millis();
}

void SuplaDeviceClass::scheduleProtocolsRestart(int timeout) {
  SUPLA_LOG_INFO("Scheduling protocols restart in %d ms", timeout);
  if (timeout <= 0) {
    protocolRestartTimeMs = 1;
  } else {
    protocolRestartTimeMs = timeout;
  }
  protocolRestartTimeoutTimestamp = millis();
}

void SuplaDeviceClass::addLastStateLog(const char *msg) {
  if (lastStateLogEnabled && lastStateLogger) {
    lastStateLogger->log(msg, uptime.getUptime());
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
        addSecurityLog(Supla::SecurityLogSource::LOCAL_DEVICE,
                       "Enter config mode from local action (button)");
        requestCfgMode();
      }
      break;
    }
    case Supla::TOGGLE_CONFIG_MODE: {
      if (deviceMode != Supla::DEVICE_MODE_CONFIG) {
        addSecurityLog(Supla::SecurityLogSource::LOCAL_DEVICE,
                       "Enter config mode from local action (button)");
        requestCfgMode();
      } else {
        scheduleSoftRestart(0);
      }
      break;
    }
    case Supla::RESET_TO_FACTORY_SETTINGS: {
      triggerResetToFactorySettings = true;
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
        addSecurityLog(Supla::SecurityLogSource::LOCAL_DEVICE,
                       "Enter config mode from local action (button)");
        requestCfgMode();
      } else if (millis() - enterConfigModeTimestamp > 2000) {
        triggerResetToFactorySettings = true;
      }
      break;
    }
    case Supla::LEAVE_CONFIG_MODE_AND_RESET: {
      if (deviceMode == Supla::DEVICE_MODE_CONFIG) {
        auto cfg = Supla::Storage::ConfigInstance();
        if (initialMode != Supla::InitialMode::StartInCfgMode ||
            (cfg && cfg->isMinimalConfigReady())) {
          addSecurityLog(Supla::SecurityLogSource::LOCAL_DEVICE,
                         "Leaving config mode, device is restarting...");
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
    cfg->setGUID(Supla::RegisterDevice::getGUID());
    cfg->setAuthKey(Supla::RegisterDevice::getAuthKey());
    cfg->initDefaultDeviceConfig();
    cfg->commit();
  }

  // cleanup state storage data
  // TODO(klew): add handling of persistant data (like energy counters)
  auto storage = Supla::Storage::Instance();
  if (storage) {
    SUPLA_LOG_DEBUG("Clearing state storage...");
    storage->deleteAll();
  }

  if (securityLogger) {
    SUPLA_LOG_DEBUG("Clearing security log...");
    securityLogger->deleteAll();
  }
}

void SuplaDeviceClass::handleLocalActionTriggers() {
  if (goToConfigModeAsap) {
    goToConfigModeAsap = false;
    enterConfigMode();
  }

  if (triggerResetToFactorySettings) {
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

void SuplaDeviceClass::checkIfLeaveCfgModeOrRestartIsNeeded() {
  uint32_t enterConfigModeTimestampCopy = enterConfigModeTimestamp;
  uint32_t deviceRestartTimeoutTimestampCopy = deviceRestartTimeoutTimestamp;
  uint32_t _millis = millis();
  uint32_t restartTimeoutValue = 5 * 60ull * 1000;
  if (isLeaveCfgModeAfterInactivityEnabled()) {
    restartTimeoutValue = leaveCfgModeAfterInactivityMin * 60ull * 1000;
  }

  // In StartWithCfgModeThenOffline device starts in "offline" mode with cfg
  // mode enabled for 1h. After that time, it will switch to full offline mode.
  // After any user interaction with www inteface, it switches to Done state.
  if (cfgModeState == Supla::CfgModeState::CfgModeStartedFor1hPending &&
      _millis > 60ULL * 60 * 1000) {
    SUPLA_LOG_INFO("Offline mode timeout triggered");
    leaveConfigModeWithoutRestart();
  }

  // CfgModeStartedPending is set when device enters cfg mode from remote
  // request (from Cloud).
  // After any interaction with www inteface, it switches to Done state.
  if ((cfgModeState == Supla::CfgModeState::CfgModeStartedPending &&
        enterConfigModeTimestampCopy != 0 &&
       _millis - enterConfigModeTimestampCopy > restartTimeoutValue)) {
    SUPLA_LOG_INFO("Config mode timeout. Leave without restart");
    leaveConfigModeWithoutRestart();
  }

  if (isLeaveCfgModeAfterInactivityEnabled() &&
      deviceRestartTimeoutTimestampCopy != 0 &&
      _millis - deviceRestartTimeoutTimestampCopy > restartTimeoutValue) {
    SUPLA_LOG_INFO("Config mode timeout. Reset device");
    softRestart();
  }

  if (forceRestartTimeMs &&
      _millis - deviceRestartTimeoutTimestampCopy > forceRestartTimeMs) {
    SUPLA_LOG_INFO("Reset requested. Reset device");
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
  if (protocolRestartTimeoutTimestamp != 0 &&
      _millis - protocolRestartTimeoutTimestamp > protocolRestartTimeMs) {
    SUPLA_LOG_INFO("Restarting connections...");
    protocolRestartTimeoutTimestamp = 0;
    protocolRestartTimeMs = 0;
    Supla::Network::DisconnectProtocols();
  }
}

const uint8_t *SuplaDeviceClass::getRsaPublicKey() const {
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

const char* SuplaDeviceClass::getSuplaCACert() const {
  if (srpcLayer) {
    return srpcLayer->getSuplaCACert();
  }
  return nullptr;
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

enum Supla::DeviceMode SuplaDeviceClass::getDeviceMode() const {
  return deviceMode;
}

Supla::Protocol::SuplaSrpc *SuplaDeviceClass::getSrpcLayer() {
  return srpcLayer;
}

void SuplaDeviceClass::setCustomHostnamePrefix(const char *prefix) {
  if (customHostnamePrefix != nullptr) {
    delete[] customHostnamePrefix;
    customHostnamePrefix = nullptr;
  }
  if (prefix == nullptr) {
    return;
  }

  int len = strlen(prefix) + 1;
  customHostnamePrefix = new char[len];
  snprintf(customHostnamePrefix, len, "%s", prefix);
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
  SUPLA_LOG_DEBUG("SD: disableNetwork");
  skipNetwork = true;
  Supla::Network::Disable();
}

void SuplaDeviceClass::enableNetwork() {
  SUPLA_LOG_DEBUG("SD: enableNetwork");
  skipNetwork = false;
  Supla::Network::SetSetupNeeded();
}

void SuplaDeviceClass::requestCfgMode() {
  goToConfigModeAsap = true;
}

bool SuplaDeviceClass::isSleepingDeviceEnabled() const {
  return Supla::RegisterDevice::isSleepingDeviceEnabled();
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
  SUPLA_LOG_WARNING("SD: allowWorkInOfflineMode is DEPRECATED");
  switch (mode) {
    default:
    case 0: {
      setInitialMode(Supla::InitialMode::StartInCfgMode);
      break;
    }
    case 1: {
      setInitialMode(Supla::InitialMode::StartOffline);
      break;
    }
    case 2: {
      setInitialMode(Supla::InitialMode::StartWithCfgModeThenOffline);
      break;
    }
  }
}

bool SuplaDeviceClass::isRemoteDeviceConfigEnabled() const {
  return Supla::RegisterDevice::isRemoteDeviceConfigEnabled();
}

void SuplaDeviceClass::enableLastStateLog() {
  lastStateLogEnabled = true;
}

void SuplaDeviceClass::disableLastStateLog() {
  lastStateLogEnabled = false;
}

void SuplaDeviceClass::setShowUptimeInChannelState(bool value) {
  showUptimeInChannelState = value;
}

void SuplaDeviceClass::setProtoVerboseLog(bool value) {
  createSrpcLayerIfNeeded();
  if (srpcLayer) {
    srpcLayer->setVerboseLog(value);
  }
}

Supla::Mutex *SuplaDeviceClass::getTimerAccessMutex() {
  return timerAccessMutex;
}

void SuplaDeviceClass::setChannelConflictResolver(
    Supla::Device::ChannelConflictResolver *resolver) {
  createSrpcLayerIfNeeded();
  srpcLayer->setChannelConflictResolver(resolver);
}

void SuplaDeviceClass::setSubdevicePairingHandler(
      Supla::Device::SubdevicePairingHandler *handler) {
  subdevicePairingHandler = handler;
}

void SuplaDeviceClass::setMacLengthInHostname(int value) {
  if (value < 0) {
    value = 0;
  }
  if (value > 6) {
    value = 6;
  }
  macLengthInHostname = value;
}

void SuplaDeviceClass::setStatusLed(Supla::Device::StatusLed *led) {
  statusLed = led;
  SUPLA_LOG_DEBUG("SD: add flag CALCFG_IDENTIFY_DEVICE");
  addFlags(SUPLA_DEVICE_FLAG_CALCFG_IDENTIFY_DEVICE);
}

void SuplaDeviceClass::setLeaveCfgModeAfterInactivityMin(int valueMin) {
  SUPLA_LOG_INFO("SD: leave cfg mode after inactivity: %d min%s",
                 valueMin, valueMin == 0 ? " (disabled)" : "");
  leaveCfgModeAfterInactivityMin = valueMin;
}

bool SuplaDeviceClass::isAutomaticFirmwareUpdateEnabled() const {
  return Supla::RegisterDevice::isAutomaticFirmwareUpdateEnabled();
}

void SuplaDeviceClass::setAutomaticFirmwareUpdateSupported(bool value) {
  if (value) {
    SUPLA_LOG_DEBUG("SD: add flag AUTOMATIC_FIRMWARE_UPDATE_SUPPORTED");
    addFlags(SUPLA_DEVICE_FLAG_AUTOMATIC_FIRMWARE_UPDATE_SUPPORTED);
    // register DeviceConfig field bit:
    Supla::Device::RemoteDeviceConfig::RegisterConfigField(
        SUPLA_DEVICE_CONFIG_FIELD_FIRMWARE_UPDATE);
  } else {
    removeFlags(SUPLA_DEVICE_FLAG_AUTOMATIC_FIRMWARE_UPDATE_SUPPORTED);
  }
}

void SuplaDeviceClass::identifyStatusLed() {
  runAction(Supla::ON_IDENTIFY);
  if (statusLed) {
    statusLed->identify();
  }
}

void SuplaDeviceClass::testStepStatusLed(int times) {
  if (statusLed) {
    statusLed->setCustomSequence(20, 50, 10, times, 1);
  }
}

void SuplaDeviceClass::setSecurityLogger(
    Supla::Device::SecurityLogger *logger) {
  securityLogger = logger;
  if (securityLogger) {
    securityLogger->init();
  }
}

void SuplaDeviceClass::addSecurityLog(uint32_t source, const char *log) const {
  if (securityLogger) {
    securityLogger->log(source, log);
  }
}

void SuplaDeviceClass::addSecurityLog(Supla::SecurityLogSource source,
                                      const char *log) const {
  if (securityLogger) {
    securityLogger->log(static_cast<uint32_t>(source), log);
  }
}

bool SuplaDeviceClass::isSecurityLogEnabled() const {
  return securityLogger && securityLogger->isEnabled();
}

void SuplaDeviceClass::setInitialMode(Supla::InitialMode mode) {
  SUPLA_LOG_INFO("SD: initial mode: %s", getInitialModeName(mode));
  initialMode = mode;
  if ((initialMode == Supla::InitialMode::StartInNotConfiguredMode ||
       initialMode == Supla::InitialMode::StartOffline ||
       initialMode == Supla::InitialMode::StartWithCfgModeThenOffline) &&
      leaveCfgModeAfterInactivityMin == 0) {
    // set default value
    setLeaveCfgModeAfterInactivityMin(5);
  }
}

const char *Supla::getInitialModeName(const Supla::InitialMode mode) {
  switch (mode) {
    case InitialMode::StartInCfgMode: {
      return "Start in cfg mode (legacy, deprected)";
    }
    case InitialMode::StartOffline: {
      return "Start offline";
    }
    case InitialMode::StartWithCfgModeThenOffline: {
      return "Start with cfg mode then offline";
    }
    case InitialMode::StartInNotConfiguredMode: {
      return "Start in not configured mode";
    }
  }
  return "Unknown";
}

bool Supla::ConfigurationState::isEmpty() const {
  // "empty" ignores wifiPassFilled because it can't be cleared manually
  // by user in cfg mode.
  bool empty = true;
  if (wifiEnabled && wifiSsidFilled) {
    empty = false;
  }
  if (protocolNotEmpty) {
    empty = false;
  }
  if (protocolFilled) {
    empty = false;
  }

  return empty;
}

bool SuplaDeviceClass::isLeaveCfgModeAfterInactivityEnabled() const {
  return leaveCfgModeAfterInactivityMin > 0 &&
         (initialMode == Supla::InitialMode::StartInNotConfiguredMode ||
          initialMode == Supla::InitialMode::StartOffline ||
          initialMode == Supla::InitialMode::StartWithCfgModeThenOffline);
}

SuplaDeviceClass SuplaDevice;
