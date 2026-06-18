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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <supla/actions.h>
#include <supla/auto_lock.h>
#include <supla/channel.h>
#include <supla/clock/clock.h>
#include <supla/device/auto_update_policy.h>
#include <supla/device/channel_conflict_resolver.h>
#include <supla/device/device_mode.h>
#include <supla/device/factory_test.h>
#include <supla/device/last_state_logger.h>
#include <supla/device/register_device.h>
#include <supla/device/remote_device_config.h>
#include <supla/device/security_logger.h>
#include <supla/device/status_led.h>
#include <supla/device/subdevice_pairing_handler.h>
#include <supla/device/sw_update.h>
#include <supla/element.h>
#include <supla/events.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>
#include <supla/mutex.h>
#include <supla/network/network.h>
#include <supla/network/web_server.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#if SUPLA_SUPLET_ENABLED
#include <supla/sha256.h>
#include <supla/suplet/capability_registry.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/registry.h>
#include <supla/suplet/server_config.h>
#endif
#include <supla/time.h>
#include <supla/timer.h>
#include <supla/tools.h>
#include <supla/version.h>

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
  deleteSupletRuntimeElements();
#if SUPLA_SUPLET_ENABLED
  if (supletCalcfgSession) {
    delete supletCalcfgSession;
    supletCalcfgSession = nullptr;
  }
  if (supletDefinitionCalcfgSession) {
    delete supletDefinitionCalcfgSession;
    supletDefinitionCalcfgSession = nullptr;
  }
#endif
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
  if (channelConflictResolvers) {
    delete channelConflictResolvers;
    channelConflictResolvers = nullptr;
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

  SUPLA_LOG_INFO("");
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
    SUPLA_LOG_INFO("");
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

    loadSupletRuntime();

    SUPLA_LOG_INFO("");
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
    SUPLA_LOG_INFO("");
    SUPLA_LOG_INFO(" *** Supla - Load state storage");
    // Pefrorm dry run of write state to validate stored state section with
    // current device configuration
    if (Supla::Storage::IsStateStorageValid()) {
      Supla::Storage::LoadStateStorage();
    }
    SUPLA_LOG_INFO(" *** Supla - Load state storage done");
  }

  SUPLA_LOG_INFO("");
  SUPLA_LOG_INFO(" *** Supla - Init elements");
  // Initialize elements
  bool isStateStorageMigrationNeeded = false;
  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onInit();
    if (element->isStateStorageMigrationNeeded()) {
      isStateStorageMigrationNeeded = true;
    }
    delay(0);
  }
  SUPLA_LOG_INFO(" *** Supla - Init elements done");
  SUPLA_LOG_INFO("");

  if (Supla::Storage::Instance() && isStateStorageMigrationNeeded) {
    SUPLA_LOG_INFO(" *** Supla - State storage migration");
    if (Supla::Storage::IsConfigStorageAvailable()) {
      Supla::Storage::ConfigInstance()->commit();
    }
    SUPLA_LOG_DEBUG("Clearing state storage...");
    Supla::Storage::Instance()->deleteAll();
    Supla::Storage::Init();
    Supla::Storage::WriteStateStorage();
    SUPLA_LOG_INFO(" *** Supla - State storage migration done");
    SUPLA_LOG_INFO("");
  }

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
    if (webServer->resolveWebServerMode() ==
        Supla::WebServer::WebServerMode::HttpsOnly) {
      // web password is used only when https is used
      SUPLA_LOG_DEBUG("SD: add flag CALCFG_SET_CFG_MODE_PASSWORD_SUPPORTED");
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
  SUPLA_LOG_INFO("");
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
  handleSupletRuntimeRefresh();
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
              Supla::AutomaticOtaCheckInterval) {
            if (initSwUpdateInstance(
                    Supla::SwUpdateMode::PeriodicCheckAndUpdate, -1)) {
              lastSwUpdateCheckTimestamp = millis();
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
        SUPLA_LOG_INFO("DEVICE_MODE_SW_UPDATE Starting SW update");
        initSwUpdateInstance(Supla::SwUpdateMode::CheckAndUpdate, 0);
      }
      iterateSwUpdate();
      if (swUpdate == nullptr) {
        deviceMode = Supla::DEVICE_MODE_NORMAL;
        if (cfg) {
          cfg->setDeviceMode(Supla::DEVICE_MODE_NORMAL);
          cfg->setSwUpdateSkipCert(false);
          cfg->setSwUpdateBeta(false);
          cfg->commit();
        }
      }
      break;
    }
  }
}

bool SuplaDeviceClass::initSwUpdateInstance(Supla::SwUpdateMode mode,
                                            int securityOnly) {
  if (swUpdate) {
    // already initialized earlier
    return true;
  }
  if (!isAutomaticFirmwareUpdateEnabled() && securityOnly != 0) {
    return false;
  }

  if (mode == Supla::SwUpdateMode::CheckAndUpdate ||
      mode == Supla::SwUpdateMode::PeriodicCheckAndUpdate) {
    swUpdateAttempts = 0;
  }

  if (mode == Supla::SwUpdateMode::RetryCheckAndUpdate) {
    swUpdateAttempts++;
    if (swUpdateAttempts > 3) {
      SUPLA_LOG_WARNING("Firmware update retry limit exceeded");
      return false;
    }
    SUPLA_LOG_INFO("Firmware update retry %d", swUpdateAttempts);
    mode = Supla::SwUpdateMode::CheckAndUpdate;
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
        this, "https://iot.updates.supla.org/check-updates", mode);
  } else {
    swUpdate = Supla::Device::SwUpdate::Create(this, url, mode);
  }
  if (swUpdate == nullptr) {
    SUPLA_LOG_WARNING("Failed to create SW update instance");
    return false;
  }

  if (cfg) {
    if (cfg->isSwUpdateBeta()) {
      swUpdate->useBeta();
    }
    if (deviceMode == Supla::DEVICE_MODE_SW_UPDATE &&
        cfg->isSwUpdateSkipCert()) {
      // Recovery-only fallback for a locally requested SW update.
      // Automatic and remotely triggered OTA checks must keep certificate
      // verification enabled even if an old recovery flag remains in storage.
      swUpdate->setSkipCert();
    }
  }
  if (securityOnly == 1) {
    swUpdate->setSecurityOnly();
  }
  if (mode == Supla::SwUpdateMode::CheckAndUpdate) {
    deviceMode = Supla::DEVICE_MODE_SW_UPDATE;
  }
  return true;
}

void SuplaDeviceClass::iterateSwUpdate() {
  if (swUpdate == nullptr) {
    return;
  }

  if (!swUpdate->isStarted()) {
    if (deviceMode == Supla::DEVICE_MODE_SW_UPDATE) {
      SUPLA_LOG_INFO("Starting SW update");
      status(STATUS_SW_DOWNLOAD, F("SW update in progress..."));
      Supla::Network::DisconnectProtocols();
    }
    swUpdate->start();
  } else {
    swUpdate->iterate();
    if (swUpdate->isAborted()) {
      SUPLA_LOG_INFO("SW update aborted");

      bool retryAllowed = swUpdate->isRetryAllowed();
      int securityPolicy = swUpdate->isSecurityOnly() ? 1 : 0;

      //      int swUpdatePolicy = swUpdate->isSecurityOnly() ? 1 : 0;
      TCalCfg_FirmwareCheckResult result = {};
      result.Result = SUPLA_FIRMWARE_CHECK_RESULT_ERROR;
      if (swUpdate->getNewVersion()) {
        SUPLA_LOG_INFO("New version available: %s", swUpdate->getNewVersion());
        strncpy(result.SoftVer,
                swUpdate->getNewVersion(),
                SUPLA_SOFTVER_MAXSIZE - 1);
        result.Result = SUPLA_FIRMWARE_CHECK_RESULT_UPDATE_AVAILABLE;
        if (swUpdate->getChangelogUrl()) {
          strncpy(result.ChangelogUrl,
                  swUpdate->getChangelogUrl(),
                  SUPLA_URL_PATH_MAXSIZE - 1);
        }
      } else if (!swUpdate->isRetryAllowed()) {
        result.Result = SUPLA_FIRMWARE_CHECK_RESULT_UPDATE_NOT_AVAILABLE;
        //        triggerSwUpdateIfAvailableAttempts = 0;
      }
      srpcLayer->sendPendingCalCfgResult(
          -1, SUPLA_CALCFG_RESULT_TRUE, -1, sizeof(result), &result);
      srpcLayer->clearPendingCalCfgResult(
          -1, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
      delete swUpdate;
      swUpdate = nullptr;
      if (retryAllowed) {
        initSwUpdateInstance(Supla::SwUpdateMode::RetryCheckAndUpdate,
                             securityPolicy);
      }
      //      if (triggerSwUpdateIfAvailableAttempts > 0) {
      //        SUPLA_LOG_INFO("Triggering SW update again (%d attempts left)",
      //                       triggerSwUpdateIfAvailableAttempts);
      //        initSwUpdateInstance(true, swUpdatePolicy);
      //      }
    } else if (swUpdate->isFinished()) {
      SUPLA_LOG_INFO("Finished SW update, restarting...");
      auto cfg = Supla::Storage::ConfigInstance();
      if (cfg) {
        cfg->setSwUpdateSkipCert(false);
        cfg->commit();
      }
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
      SUPLA_LOG_DEBUG("New AuthKey generated");
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

#if SUPLA_SUPLET_ENABLED
namespace {

uint8_t supletStateToCalcfg(Supla::Suplet::InstanceState state) {
  switch (state) {
    case Supla::Suplet::InstanceState::Active:
      return SUPLA_CALCFG_SUPLET_INSTANCE_STATE_ACTIVE;
    case Supla::Suplet::InstanceState::Staged:
      return SUPLA_CALCFG_SUPLET_INSTANCE_STATE_STAGED;
    case Supla::Suplet::InstanceState::DeletePending:
      return SUPLA_CALCFG_SUPLET_INSTANCE_STATE_DELETE_PENDING;
    case Supla::Suplet::InstanceState::Disabled:
    default:
      return SUPLA_CALCFG_SUPLET_INSTANCE_STATE_DISABLED;
  }
}

Supla::Suplet::InstanceState supletStateFromCalcfg(uint8_t state) {
  switch (state) {
    case SUPLA_CALCFG_SUPLET_INSTANCE_STATE_ACTIVE:
      return Supla::Suplet::InstanceState::Active;
    case SUPLA_CALCFG_SUPLET_INSTANCE_STATE_STAGED:
      return Supla::Suplet::InstanceState::Staged;
    case SUPLA_CALCFG_SUPLET_INSTANCE_STATE_DELETE_PENDING:
      return Supla::Suplet::InstanceState::DeletePending;
    case SUPLA_CALCFG_SUPLET_INSTANCE_STATE_DISABLED:
    default:
      return Supla::Suplet::InstanceState::Disabled;
  }
}

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

}  // namespace
#endif

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
      case SUPLA_CALCFG_CMD_SUPLET_GET_CAPABILITIES:
      case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_COUNT:
      case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_LIST:
      case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_INFO:
      case SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_CONFIG:
      case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN:
      case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_CHUNK:
      case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_COMMIT:
      case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_ABORT:
      case SUPLA_CALCFG_CMD_SUPLET_DEFINITION_REMOVE:
      case SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_LIST:
      case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN:
      case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_CHUNK:
      case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_COMMIT:
      case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_REMOVE:
      case SUPLA_CALCFG_CMD_SUPLET_INSTANCE_ABORT: {
        return handleSupletCalcfg(request, result);
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
        initSwUpdateInstance(Supla::SwUpdateMode::OnlyCheck, 0);
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

        if (initSwUpdateInstance(Supla::SwUpdateMode::CheckAndUpdate, 0)) {
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
        if (initSwUpdateInstance(Supla::SwUpdateMode::CheckAndUpdate, 1)) {
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
#ifndef ARDUINO_ARCH_AVR
        Supla::Config::generateSaltPassword(password->NewPassword,
                                            &saltPassword);
#else
        return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
#endif
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

int SuplaDeviceClass::handleSupletCalcfg(TSD_DeviceCalCfgRequest *request,
                                         TDS_DeviceCalCfgResult *result) {
#if SUPLA_SUPLET_ENABLED
  if (request == nullptr || result == nullptr || supletManager == nullptr ||
      supletRegistry == nullptr || supletServerConfigHandler == nullptr) {
    fillSupletResult(result,
                     SUPLA_CALCFG_SUPLET_RESULT_UNSUPPORTED_DEFINITION,
                     SUPLA_CALCFG_SUPLET_PHASE_NONE);
    return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
  }

  auto table = supletManager->getInstanceTable();
  if (table == nullptr) {
    fillSupletResult(result,
                     SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR,
                     SUPLA_CALCFG_SUPLET_PHASE_NONE);
    return SUPLA_CALCFG_RESULT_FALSE;
  }

  switch (request->Command) {
    case SUPLA_CALCFG_CMD_SUPLET_GET_CAPABILITIES: {
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
      const uint8_t total =
          supletServerConfigHandler->getCachedDefinitionCount();
      output.Total = total;
      uint8_t limit = listRequest.Limit;
      if (limit == 0 || limit > SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS) {
        limit = SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS;
      }
      for (uint8_t i = 0; i < limit && listRequest.Offset + i < total; i++) {
        Supla::Suplet::CachedDefinitionDetails details = {};
        if (!supletServerConfigHandler->getCachedDefinitionDetails(
                listRequest.Offset + i, &details)) {
          break;
        }
        auto &item = output.Items[output.Count++];
        item.Category = static_cast<uint8_t>(details.category);
        item.Kind = static_cast<uint8_t>(details.kind);
        item.SchemaVersion = details.schemaVersion;
        item.HandlerVersion = details.handlerVersion;
        item.MaxInstances = details.maxInstances;
        item.DefinitionId = details.cache.definitionId;
        item.DefinitionVersion = details.cache.definitionVersion;
        item.JsonSize = details.cache.jsonSize;
        memcpy(item.JsonSha256, details.cache.sha256, sizeof(item.JsonSha256));
      }
      memcpy(result->Data, &output, sizeof(output));
      result->DataSize = sizeof(output);
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
        item.State = supletStateToCalcfg(record->state);
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
      output.State = supletStateToCalcfg(record->state);
      output.SubDeviceId = record->subDeviceId;
      output.ParamsSize = record->configSize;
      calculateSha256(record->config, record->configSize, output.ParamsSha256);
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
        memcpy(output.Data, record->config + configRequest.Offset, output.Size);
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
      supletDefinitionCalcfgSession =
          new Supla::Suplet::DefinitionCalcfgSession();
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
      supletDefinitionCalcfgSession->definitionId = begin.DefinitionId;
      supletDefinitionCalcfgSession->definitionVersion =
          begin.DefinitionVersion;
      supletDefinitionCalcfgSession->jsonSize = begin.JsonSize;
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
      memcpy(supletDefinitionCalcfgSession->json + chunk.Offset,
             chunk.Data,
             chunk.Size);
      for (uint8_t i = 0; i < chunk.Size; i++) {
        uint16_t index = chunk.Offset + i;
        if (!supletDefinitionCalcfgSession->received[index]) {
          supletDefinitionCalcfgSession->received[index] = 1;
          supletDefinitionCalcfgSession->receivedSize++;
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
      uint8_t calculatedSha[32] = {};
      calculateSha256(reinterpret_cast<const uint8_t *>(
                          supletDefinitionCalcfgSession->json),
                      supletDefinitionCalcfgSession->jsonSize,
                      calculatedSha);
      if (memcmp(calculatedSha,
                 supletDefinitionCalcfgSession->expectedSha256,
                 sizeof(calculatedSha)) != 0) {
        fillSupletResult(result,
                         SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_SHA_MISMATCH,
                         SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE,
                         0,
                         supletDefinitionCalcfgSession->definitionId,
                         supletDefinitionCalcfgSession->definitionVersion);
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      supletDefinitionCalcfgSession
          ->json[supletDefinitionCalcfgSession->jsonSize] = '\0';
      auto serverResult = supletServerConfigHandler->saveDownloadedDefinition(
          supletDefinitionCalcfgSession->definitionId,
          supletDefinitionCalcfgSession->definitionVersion,
          supletDefinitionCalcfgSession->json,
          supletDefinitionCalcfgSession->expectedSha256);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_PARSE_TEMPLATE,
                       0,
                       supletDefinitionCalcfgSession->definitionId,
                       supletDefinitionCalcfgSession->definitionVersion);
      delete supletDefinitionCalcfgSession;
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
        delete supletDefinitionCalcfgSession;
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
      supletCalcfgSession = new Supla::Suplet::InstanceCalcfgSession();
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
      supletCalcfgSession->instanceId = begin.InstanceId;
      supletCalcfgSession->definitionId = begin.DefinitionId;
      supletCalcfgSession->definitionVersion = begin.DefinitionVersion;
      supletCalcfgSession->paramsSize = begin.ParamsSize;
      supletCalcfgSession->state = supletStateFromCalcfg(begin.State);
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
      Supla::Suplet::ChannelAllocator occupied;
      fillSupletOccupiedChannels(&occupied);
      supletCalcfgSession->params[supletCalcfgSession->paramsSize] = '\0';
      uint8_t appliedInstanceId = supletCalcfgSession->instanceId;
      auto serverResult = supletServerConfigHandler->applyInstanceParams(
          supletCalcfgSession->instanceId,
          supletCalcfgSession->definitionId,
          supletCalcfgSession->definitionVersion,
          supletCalcfgSession->state,
          reinterpret_cast<const char *>(supletCalcfgSession->params),
          supletCalcfgSession->paramsSize,
          occupied,
          &appliedInstanceId);
      fillSupletResult(result,
                       supletDetailFromServerResult(serverResult),
                       SUPLA_CALCFG_SUPLET_PHASE_SAVE_INSTANCE,
                       appliedInstanceId,
                       supletCalcfgSession->definitionId,
                       supletCalcfgSession->definitionVersion);
      delete supletCalcfgSession;
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
        delete supletCalcfgSession;
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
#else
  (void)(request);
  (void)(result);
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
#endif
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
  if (timeSec < 60) {
    // disabled
    timeSec = 0;
  }
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

const char *SuplaDeviceClass::getSuplaCACert() const {
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

void SuplaDeviceClass::setLogLevel(int level) {
  supla_log_set_level(level);
}

int SuplaDeviceClass::getLogLevel() {
  return supla_log_get_level();
}

void SuplaDeviceClass::setProtoVerboseLog(bool value) {
  createSrpcLayerIfNeeded();
  if (srpcLayer) {
    if (value) {
      SUPLA_LOG_WARNING(
          "Protocol verbose logging enabled (INSECURE): may expose secrets "
          "and raw protocol payloads");
    }
    srpcLayer->setVerboseLog(value);
    srpcLayer->setLowLevelDebugLogs(value);
  }
}

Supla::Mutex *SuplaDeviceClass::getTimerAccessMutex() {
  return timerAccessMutex;
}

void SuplaDeviceClass::setChannelConflictResolver(
    Supla::Device::ChannelConflictResolver *resolver) {
  createSrpcLayerIfNeeded();
  if (channelConflictResolvers == nullptr) {
    channelConflictResolvers = new Supla::Device::ChannelConflictResolverList;
  }
  if (channelConflictResolvers != nullptr) {
    channelConflictResolvers->clear();
    channelConflictResolvers->add(resolver);
    srpcLayer->setChannelConflictResolver(channelConflictResolvers->isEmpty()
                                              ? nullptr
                                              : channelConflictResolvers);
  } else {
    srpcLayer->setChannelConflictResolver(resolver);
  }
}

void SuplaDeviceClass::setSupletRuntime(Supla::Suplet::Manager *manager,
                                        Supla::Suplet::Registry *registry) {
#if SUPLA_SUPLET_ENABLED
  supletManager = manager;
  supletRegistry = registry;
  if (supletManager != nullptr && supletRegistry != nullptr &&
      supletServerConfigHandler != nullptr) {
    addFlags(SUPLA_DEVICE_FLAG_SUPLET_SUPPORTED);
  } else {
    removeFlags(SUPLA_DEVICE_FLAG_SUPLET_SUPPORTED);
  }
#else
  (void)(manager);
  (void)(registry);
#endif
}

void SuplaDeviceClass::setSupletCapabilityRegistry(
    Supla::Suplet::CapabilityRegistry *registry) {
#if SUPLA_SUPLET_ENABLED
  supletCapabilityRegistry = registry;
#else
  (void)(registry);
#endif
}

void SuplaDeviceClass::setSupletServerConfigHandler(
    Supla::Suplet::ServerConfigHandler *handler) {
#if SUPLA_SUPLET_ENABLED
  supletServerConfigHandler = handler;
  if (supletManager != nullptr && supletRegistry != nullptr &&
      supletServerConfigHandler != nullptr) {
    addFlags(SUPLA_DEVICE_FLAG_SUPLET_SUPPORTED);
  } else {
    removeFlags(SUPLA_DEVICE_FLAG_SUPLET_SUPPORTED);
  }
#else
  (void)(handler);
#endif
}

Supla::Suplet::ServerConfigResult SuplaDeviceClass::applySupletCommandJson(
    const char *commandJson) {
#if SUPLA_SUPLET_ENABLED
  if (supletServerConfigHandler == nullptr) {
    return Supla::Suplet::ServerConfigResult::InvalidArgument;
  }
  Supla::Suplet::ChannelAllocator occupied;
  if (!fillSupletOccupiedChannels(&occupied)) {
    return Supla::Suplet::ServerConfigResult::InvalidArgument;
  }
  return supletServerConfigHandler->applyCommandJson(commandJson, occupied);
#else
  (void)(commandJson);
  return static_cast<Supla::Suplet::ServerConfigResult>(2);
#endif
}

Supla::Suplet::ServerConfigResult SuplaDeviceClass::validateSupletCommandJson(
    const char *commandJson) const {
#if SUPLA_SUPLET_ENABLED
  if (supletServerConfigHandler == nullptr) {
    return Supla::Suplet::ServerConfigResult::InvalidArgument;
  }
  Supla::Suplet::ChannelAllocator occupied;
  if (!fillSupletOccupiedChannels(&occupied)) {
    return Supla::Suplet::ServerConfigResult::InvalidArgument;
  }
  return supletServerConfigHandler->validateCommandJson(commandJson, occupied);
#else
  (void)(commandJson);
  return static_cast<Supla::Suplet::ServerConfigResult>(2);
#endif
}

bool SuplaDeviceClass::loadSupletRuntime() {
#if SUPLA_SUPLET_ENABLED
  if (supletManager == nullptr || supletRegistry == nullptr) {
    return true;
  }

  addChannelConflictResolver(supletManager);
  deleteSupletRuntimeElements();

  if (!supletManager->load()) {
    SUPLA_LOG_DEBUG("Suplet runtime: no stored instance table");
    supletCreatedElementCount = 0;
    return true;
  }

  supletCreatedElementCount = 0;
  if (!supletManager->createElementsFromRegistry(
          *supletRegistry,
          supletCreatedElements,
          sizeof(supletCreatedElements) / sizeof(supletCreatedElements[0]),
          &supletCreatedElementCount)) {
    SUPLA_LOG_WARNING("Suplet runtime: failed to create stored elements");
    supletCreatedElementCount = 0;
    return false;
  }

  if (supletCreatedElementCount > 0) {
    SUPLA_LOG_INFO("Suplet runtime: created %u element(s)",
                   supletCreatedElementCount);
  }
  return true;
#else
  return true;
#endif
}

bool SuplaDeviceClass::handleSupletRuntimeRefresh() {
#if SUPLA_SUPLET_ENABLED
  if (supletServerConfigHandler == nullptr ||
      !supletServerConfigHandler->isRuntimeRefreshRequired()) {
    return false;
  }

  SUPLA_LOG_INFO("Suplet runtime: refreshing elements after config change");
  bool result = loadSupletRuntime();
  if (result) {
    for (uint16_t i = 0; i < supletCreatedElementCount; i++) {
      if (supletCreatedElements[i] != nullptr) {
        if (Supla::Storage::IsConfigStorageAvailable()) {
          supletCreatedElements[i]->onLoadConfig(this);
        }
        supletCreatedElements[i]->onInit();
      }
      delay(0);
    }
    iterateConnectedPtr = nullptr;
    Supla::Network::DisconnectProtocols();
  }
  supletServerConfigHandler->clearRuntimeRefreshRequired();
  return result;
#else
  return false;
#endif
}

void SuplaDeviceClass::deleteSupletRuntimeElements() {
#if SUPLA_SUPLET_ENABLED
  for (uint16_t i = 0; i < supletCreatedElementCount; i++) {
    delete supletCreatedElements[i];
    supletCreatedElements[i] = nullptr;
  }
  supletCreatedElementCount = 0;
#endif
}

bool SuplaDeviceClass::fillSupletOccupiedChannels(
    Supla::Suplet::ChannelAllocator *allocator) const {
#if SUPLA_SUPLET_ENABLED
  if (allocator == nullptr) {
    return false;
  }
  allocator->clearOccupied();
  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    int channelNumber = element->getChannelNumber();
    if (channelNumber >= 0 && !allocator->markOccupied(channelNumber)) {
      return false;
    }
    channelNumber = element->getSecondaryChannelNumber();
    if (channelNumber >= 0 && !allocator->markOccupied(channelNumber)) {
      return false;
    }
  }
  return true;
#else
  (void)(allocator);
  return false;
#endif
}

bool SuplaDeviceClass::addChannelConflictResolver(
    Supla::Device::ChannelConflictResolver *resolver) {
  if (resolver == nullptr) {
    return false;
  }
  createSrpcLayerIfNeeded();
  if (channelConflictResolvers == nullptr) {
    channelConflictResolvers = new Supla::Device::ChannelConflictResolverList;
  }
  if (channelConflictResolvers == nullptr) {
    return false;
  }
  bool result = channelConflictResolvers->add(resolver);
  srpcLayer->setChannelConflictResolver(channelConflictResolvers);
  return result;
}

bool SuplaDeviceClass::removeChannelConflictResolver(
    Supla::Device::ChannelConflictResolver *resolver) {
  if (channelConflictResolvers == nullptr || resolver == nullptr) {
    return false;
  }
  bool result = channelConflictResolvers->remove(resolver);
  if (srpcLayer != nullptr && channelConflictResolvers->isEmpty()) {
    srpcLayer->setChannelConflictResolver(nullptr);
  }
  return result;
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
                 valueMin,
                 valueMin == 0 ? " (disabled)" : "");
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
