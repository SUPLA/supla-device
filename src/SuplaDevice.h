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

#ifndef SRC_SUPLADEVICE_H_
#define SRC_SUPLADEVICE_H_

#include <supla-common/proto.h>
#include <supla/uptime.h>
#include <supla/action_handler.h>
#include <supla/local_action.h>
#include <supla/device/auto_update_mode.h>
#include <supla/device/device_mode.h>

#define STATUS_UNKNOWN                   -1
#define STATUS_ALREADY_INITIALIZED       1
#define STATUS_MISSING_NETWORK_INTERFACE 2
#define STATUS_UNKNOWN_SERVER_ADDRESS    3
#define STATUS_UNKNOWN_LOCATION_ID       4
#define STATUS_INITIALIZED               5
#define STATUS_SERVER_DISCONNECTED       6
#define STATUS_ITERATE_FAIL              7
#define STATUS_NETWORK_DISCONNECTED      8
#define STATUS_ALL_PROTOCOLS_DISABLED    9

#define STATUS_REGISTER_IN_PROGRESS      10  // Don't change
#define STATUS_REGISTERED_AND_READY      17  // Don't change

#define STATUS_TEMPORARILY_UNAVAILABLE   21
#define STATUS_INVALID_GUID              22
#define STATUS_CHANNEL_LIMIT_EXCEEDED    23
#define STATUS_PROTOCOL_VERSION_ERROR    24
#define STATUS_BAD_CREDENTIALS           25
#define STATUS_LOCATION_CONFLICT         26
#define STATUS_CHANNEL_CONFLICT          27
#define STATUS_DEVICE_IS_DISABLED        28
#define STATUS_LOCATION_IS_DISABLED      29
#define STATUS_DEVICE_LIMIT_EXCEEDED     30
#define STATUS_REGISTRATION_DISABLED     31
#define STATUS_MISSING_CREDENTIALS       32
#define STATUS_INVALID_AUTHKEY           33
#define STATUS_NO_LOCATION_AVAILABLE     34
#define STATUS_UNKNOWN_ERROR             35
#define STATUS_COUNTRY_REJECTED          36

#define STATUS_CONFIG_MODE               40
#define STATUS_SOFTWARE_RESET            41
#define STATUS_SW_DOWNLOAD               50
#define STATUS_SUPLA_PROTOCOL_DISABLED   60
#define STATUS_TEST_WAIT_FOR_CFG_BUTTON  70
#define STATUS_OFFLINE_MODE              80

// 10 days
#define SUPLA_AUTOMATIC_OTA_CHECK_INTERVAL (10ULL * 24 * 60 * 60 * 1000)

typedef void (*_impl_arduino_status)(int status, const char *msg);

#ifdef ARDUINO
class __FlashStringHelper;
#else
#define __FlashStringHelper char
#endif

namespace Supla {
class Clock;
class Mutex;
class Element;

namespace Device {
class SwUpdate;
class ChannelConflictResolver;
class SubdevicePairingHandler;
class StatusLed;
class LastStateLogger;
}  // namespace Device

namespace Protocol {
class SuplaSrpc;
}  // namespace Protocol

}  // namespace Supla

class SuplaDeviceClass : public Supla::ActionHandler,
  public Supla::LocalAction {
 public:
  SuplaDeviceClass();
  ~SuplaDeviceClass();

  void fillStateData(TDSC_ChannelState *channelState);
  void addClock(Supla::Clock *clock);  // DEPRECATED
  Supla::Clock *getClock();

  bool begin(const char GUID[SUPLA_GUID_SIZE],
             const char *Server,
             const char *email,
             const char authkey[SUPLA_AUTHKEY_SIZE],
             unsigned char protoVersion = 23);

  bool begin(unsigned char protoVersion = 23);

  // Use ASCII only in name
  void setName(const char *Name);

  void setGUID(const char GUID[SUPLA_GUID_SIZE]);
  void setAuthKey(const char authkey[SUPLA_AUTHKEY_SIZE]);
  void setEmail(const char *email);
  void setServer(const char *server);
  void setSwVersion(const char *);

  // Do not set the manufacturer ID to anything other than 0. Values other than
  // 0 are reserved for official Supla products only
  void setManufacturerId(int16_t);

  // Do not set the product ID to anything other than 0. Values other than
  // 0 are reserved for official Supla products only
  void setProductId(int16_t);

  void addFlags(int32_t);
  void removeFlags(int32_t);
  bool isSleepingDeviceEnabled();

  int generateHostname(char*, int macSize = 6);

  // Timer with 100 Hz frequency (10 ms)
  void onTimer(void);
  // Timer with 2000 Hz frequency (0.5 ms or 1 ms)
  void onFastTimer(void);
  void iterate(void);

  void status(int status,
              const __FlashStringHelper *msg,
              bool alwaysLog = false);
  void setStatusFuncImpl(_impl_arduino_status impl_arduino_status);
  void setServerPort(int value);

  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request,
                             TDS_DeviceCalCfgResult *result = nullptr);

  void enterConfigMode();
  void leaveConfigModeWithoutRestart();
  void enterNormalMode();
  // Schedules timeout to restart device. When provided timeout is 0
  // then restart will be done asap.
  void scheduleSoftRestart(int timeout = 0);
  void scheduleProtocolsRestart(int timeout = 0);
  void softRestart();
  void saveStateToStorage();
  void restartCfgModeTimeout(bool requireRestart);
  void resetToFactorySettings();
  void disableLocalActionsIfNeeded();
  void requestCfgMode();

  int getCurrentStatus();
  bool loadDeviceConfig();
  bool prepareLastStateLog();
  char *getLastStateLog();
  void addLastStateLog(const char*);
  void enableLastStateLog();
  void disableLastStateLog();
  void setRsaPublicKeyPtr(const uint8_t *ptr);
  const uint8_t *getRsaPublicKey();
  enum Supla::DeviceMode getDeviceMode() const;

  void setActivityTimeout(_supla_int_t newActivityTimeout);
  uint32_t getActivityTimeout();

  void handleAction(int event, int action) override;

  // Enables automatic software reset of device in case of network/server
  // connection problems longer than timeSec.
  // timeSec is always round down to multiplication of 10 s.
  // timeSec <= 60 will disable automatic restart.
  void setAutomaticResetOnConnectionProblem(unsigned int timeSec);

  void setLastStateLogger(Supla::Device::LastStateLogger *logger);

  void setSuplaCACert(const char *);
  void setSupla3rdPartyCACert(const char *);
  const char* getSuplaCACert() const;

  Supla::Uptime uptime;

  Supla::Protocol::SuplaSrpc *getSrpcLayer();

  void setCustomHostnamePrefix(const char *prefix);

  void enableNetwork();
  void disableNetwork();
  bool getStorageInitResult();
  bool isSleepingAllowed();

  // Call this method if you want to allow device to work in offline mode
  // without Wi-Fi network configuration
  // 1 - offline mode with empty config, but communication protocols may be
  //     enabled
  // 2 - offline mode only with empty config and communication protocols
  //     disabled
  // 0 - no offline mode
  void allowWorkInOfflineMode(int mode = 1);

  bool isRemoteDeviceConfigEnabled() const;
  void setShowUptimeInChannelState(bool value);

  void setProtoVerboseLog(bool value);
  bool isOfflineModeDuringConfig() const;

  Supla::Mutex *getTimerAccessMutex();

  void setChannelConflictResolver(
      Supla::Device::ChannelConflictResolver *resolver);
  void setSubdevicePairingHandler(
      Supla::Device::SubdevicePairingHandler *handler);

  void setMacLengthInHostname(int value);

  void setStatusLed(Supla::Device::StatusLed *led);

  /**
   * Sets the leave configuration mode after inactivity timeout in minutes.
   * Device will automatically leave configuration mode after specified
   * inactivity timeout (no HTTP GET/POST requests).
   *
   * @param valueMin inactivity timeout in minutes. Use 0 to disable
   */
  void setLeaveCfgModeAfterInactivityMin(int valueMin);

  /**
   * Checks if automatic firmware update is supported
   *
   * @return true if automatic update is supported
   */
  bool isAutomaticFirmwareUpdateEnabled() const;

  /**
   * Sets automatic firmware update support
   *
   * @param value true to enable, false to disable
   */
  void setAutomaticFirmwareUpdateSupported(bool value);

  /**
   * Returns current automatic firmware update mode
   *
   * @return current automatic firmware update mode
   */
  Supla::AutoUpdateMode getAutoUpdateMode() const;

 protected:
  /**
   * Performs software update if needed
   *
   */
  void iterateSwUpdate();

  /**
   * Initializes SW update instance
   *
   * @param performUpdate true to perform SW update, false to check for update
   * @param securityOnly 0 to check for all updates, 1 to check for security
   *                       updates, -1 will use value from device config
   *
   * @return true if SW update instance was initialized
   */
  bool initSwUpdateInstance(bool performUpdate, int securityOnly = -1);

  int networkIsNotReadyCounter = 0;

  uint32_t deviceRestartTimeoutTimestamp = 0;
  uint32_t protocolRestartTimeoutTimestamp = 0;
  uint32_t waitForIterate = 0;
  uint32_t lastIterateTime = 0;
  uint32_t enterConfigModeTimestamp = 0;
  uint32_t forceRestartTimeMs = 0;
  uint32_t protocolRestartTimeMs = 0;
  uint32_t resetOnConnectionFailTimeoutSec = 0;
  uint32_t lastSwUpdateCheckTimestamp = 0;
  int allowOfflineMode = 1;
  int currentStatus = STATUS_UNKNOWN;

  enum Supla::DeviceMode deviceMode = Supla::DEVICE_MODE_NOT_SET;
  bool triggerResetToFactorySettings = false;
  bool triggerStartLocalWebServer = false;
  bool triggerStopLocalWebServer = false;
  bool triggerCheckSwUpdate = false;
  bool requestNetworkLayerRestart = false;
  bool isNetworkSetupOk = false;
  bool skipNetwork = false;
  bool storageInitResult = false;
  bool configEmpty = true;
  bool atLeastOneProtoIsEnabled = false;
  bool showUptimeInChannelState = true;
  bool lastStateLogEnabled = true;
  // used to indicate if begin() method was called - it will be set to
  // true even if initialization procedure failed for some reason
  bool initializationDone = false;
  bool goToConfigModeAsap = false;
  bool triggerSwUpdateIfAvailable = false;

  uint8_t goToOfflineModeTimeout = 0;
  uint8_t leaveCfgModeAfterInactivityMin = 5;
  uint8_t macLengthInHostname = 6;

  Supla::Protocol::SuplaSrpc *srpcLayer = nullptr;
  Supla::Device::SwUpdate *swUpdate = nullptr;
  const uint8_t *rsaPublicKey = nullptr;
  Supla::Element *iterateConnectedPtr = nullptr;
  _impl_arduino_status impl_arduino_status = nullptr;
  Supla::Device::LastStateLogger *lastStateLogger = nullptr;
  char *customHostnamePrefix = nullptr;
  Supla::Mutex *timerAccessMutex = nullptr;
  Supla::Device::SubdevicePairingHandler *subdevicePairingHandler = nullptr;
  Supla::Device::StatusLed *statusLed = nullptr;

  void iterateAlwaysElements(uint32_t _millis);
  bool iterateNetworkSetup();
  bool iterateSuplaProtocol(uint32_t _millis);
  void handleLocalActionTriggers();
  void checkIfRestartIsNeeded(uint32_t _millis);
  void createSrpcLayerIfNeeded();
  void setupDeviceMode();
};

extern SuplaDeviceClass SuplaDevice;
#endif  // SRC_SUPLADEVICE_H_
