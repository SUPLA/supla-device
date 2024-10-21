# CHANGELOG.md

## 24.10.04 (2024-10-18 SOP edition)
   - Change: configuration tags (names used in internal config storage) are extracted from HTML related classes and moved to ./storage/config_tags.h file in order to cleanup some mess and dependency to HTML classes in other parts of code
   - Change: BlinkingLed: extracted base functionality from StatusLed class, so BlinkingLed can be used for purposes not related with device's status.
   - Change: OcrImpulseCounter: change default and minimum PhotoInterval to 60 min
   - Change: ElectricityMeter: in case of missing measurement, we set m_count to 0 and leave measured_values untouched
   - Change: Change CALCFG reply handling from "FALSE" to "NOT_SUPPROTED" for situations where received command is not supported by a class.
   - Fix: GPM: set setChannelRefreshIntervalMs as virtual
   - Fix: ElectricityMeter: fix kW/kvar/kVA units for high negative values of power
   - Fix: ElectricityMeter: fix refreshRate setting, so it will also affect how often extended value is send with measurements
   - Fix: BinarySensor: apply server's inverted logic to ON_TURN_ON and ON_TURN_OFF events
   - Fix: DS1307 & DS3231 RTC: fix missing RTC update in some rare scenario
   - Add: HVAC: add "pump switch" and "heat or cold source switch" support
   - Add: HVAC: add handling of master thermostat setting. Thermostats with configured master thermostat will ignore server and local (button) changes, and they will be overriden by master thermostat settings.  Mode and temperature setpoints are copied. If mode is not supported, thermostat is turned OFF.
   - Add: HVAC Channel: add support for "battery cover open" HVAC value flag
   - Add: GroupButtonControlRgbw: add setButtonControlType(rgbwIndex, type) to configure it in code instead of Config
   - Add: RelayHvacAggregator: add class to aggregate HVAC status and control relay output based on heating state
   - Add: Channel: add "offline=2" variant which informs server that channel in "online", but it is not available i.e. beacuse of specific configuration.
   - Add: esp-idf: add InterruptAcToDcIo class to handle "AC alternating" input handling.
   - Add: MQTT: add roller shutter (and similar functions) support for MQTT Autodiscovery Home Assistant
   - Add: Channel: add getters for roller shutter value.
   - Add: RollerShutter: add support for Config exchange with server, add handling of motor/buttons upside down, add handling for time margin setting, add support for RS related functions: terrace awning, projector screen, roof window, roller garage door, curtain.
   - Add: RollerShutter: add HTML component for configuration
   - Add: RollerShutter: add CALCFG recalibrate support. Add "calibration in progress" flag report.
   - Add: SRPC layer: add sending channel state (with battery level) as extended value message after registration and periodically every 1h for battery powered channels.
   - Add: Channel: add setBatteryPowered() method to indicate battery powered device without knowing exact battery level.
   - Add: Element: add isOwnerOfSubDeviceId method to get element reponsible for specific subdevice handling.

## 24.09 (2024-09-05)
   - Fix: Relay: change int to int32_t (potentially problem on Arduino Mega)
   - Fix: HVAC: small mem usage reduction
   - Fix: HVAC: add validation of stored THVACValues (currently only with warning log)
   - Fix: HVAC: fix "countDownTimerEnds" initial value causing inifinte sending of remaining timer value to server
   - Fix: HVAC: fix invalid initialization of some parameters
   - Fix: WearLevelingSector: fix storage write in case of storage sector size change (i.e. when new channel was added online)
   - Fix: HTML ProtocolParameters: fix HTML when addMqtt is false and conqurent is false
   - Fix: SuplaOcrIc: fix config sending after device removal on Cloud
   - Fix: SuplaOcrIc: fix picture fetching sequence, so we'll get latest photo instead of previous one. esp_camera is taking photos when photo buffer is empty (at esp_camera_fb_return and not at esp_camer_fb_get)
   - Change: Log improvements
   - Add: Storage: add enableChannelNumbers() option to store channel number first, before channel state data. This mode will keep state only for elements with valid channel number.
   - Add: GroupButtonControlRgbw: add setButtonControlType(rgbwIndex, type)
   - Add: Relay: add support for power and light switch channel config. Adjusted few classes to new handling of config for common usage in ElementWithChannelActions class.
   - Add: esp-idf NvsConfig: add option to use "suplanvs" partition for configuration storage
   - Add: SuplaDevice: add setMacLengthInHostname(int) to configure length of MAC part in hostname/WiFi AP name
   - Add: SuplaDevice: add handling of SUPLA_CALCFG_CMD_RESTART_DEVICE. Add bool isDeviceSoftwareResetSupported() method.


## 24.08.01 (2024-08-07)
  - Version increased to 24.08.01 for internal use

## 24.08 (2024-08-07)

  - Fix: Arduino ESP32: fix for generating device hostname and softAP name which is based on device's mac address. On ESP32 3.0.0 boards onwards, WiFi.macAddress() returns 0 when it's called before initialization of WiFi.
  - Fix: HTML: rename current transformer "133.2mA" to "133.3mA"
  - Fix: sd4linux: fix for rare random hang on connect()
  - Fix: Arduino IDE (ESP32): ETH classes update, compilation fix for ESP32 Arduino boards version 3.x,  wt32_eth01: code adjustment to changes in Network classes (thanks @lukfud)
  - Fix: StatusLed: fix not updateing status led settings when it was changed via cfg mode
  - Fix: StatusLed: ignore "always off" setting for config mode and sw update mode
  - Fix: ElectricityMeter: fix invalid handling of server channel config for CT type and PhaseLedType
  - Fix: Clock: moved "isReady" flag setting to last step of paring time from server.
  - Fix: HVAC: add extra weekly schedule initialization in "onInit" in order to apply proper program based on clock availability in early startup.
  - Change: ElectricityMeter, SolarEdge integration: change refreshRateSec to uint16_t, to allow 6 min refresh rate set in SolarEdge
  - Change: SuplaDevice: enforcing proto version >= 23
  - Change: HVAC: move setOutput() method to protected section. Allow setting negative value to OutputInterface in setOutputValue, adjusted getOutputValue to return last set output value.
  - Change: Hvac: outputs working in 0..100% mode, will report channel value IsOn as 0 or 2..101 in order to distinguish on/off (1/0) mode from 0..100% mode
  - Change: ImpulseCounter: change base class to VirtualImpulseCounter and make implementation shared.
  - Add: ESP32-LAN-SPI support with Arduino IDE examples (thanks @ermaya)
  - Add: Tools: add Supla::rssiToSignalStrength(int) method
  - Add: SuplaDevice: add timer execution/access mutex to prevent concurrent element creation/deletion and timer execution on it
  - Add:Channel: add isOnline() method
  - Add: Config: add eraseKey(key) method
  - Add: Thermometer, ThermHygroMeter: add purgeConfig method to cleanup config structures related to current class.
  - Add: Channel: setChannelNumber will swap channel numbers if we try to set it to value already used by other channel. Add isFunctionValid for channel function validation against channel type (currently implemented only for binary sensors).
  - Add: BinaryBase: add purgeConfig() implementation
  - Add: Channel: add bridge signal strength parameter to channel state report.
  - Add: FactoryTest: suspend test, when device entered cfg mode.
  - Add: PowerCycle: enable power cycle eneter to config mode, when device mode is not "normal" (i.e. when factory test is enabled)
  - Add: support for registerdevice_g variant (with SubDeviceId) - requires proto >= 25
  - Add: ActionTrigger: add isAnyActionEnabledOnServer() getter
  - Add: Button: add getLastStateChange() method for getting last timestamp of change
  - Add: logs with detailed information about channel conflict from server (proto >= 25).
  - Add: ChannelConflictResolver interface class - if device is able to resolve conflicts in some way, this functionality can be implemented in this class
  - Add: HvacBase: add ability to set channel state based on OutputInterface values (i.e. from external thermostat)
  - Add: EnterCfgModeAfterPowerCycle: add option to always enable this function, regardless of other config parameter values - add parameter in ctr and explicit method setAlwaysEnabled(bool)
  - Add: RemoteDeviceConfig: add support for PowerStatusLed option.
  - Add: Html: add form for PowerStatusLed settings
  - Add: HvacBase: add parameterFlags for readonly and hidden properties
  - Add: HVAC: add option to mark parameter as readonly (it's change won't be accepted and device will send back corrected config to server), add option to set parameter as hidden (for Cloud and also used in local cfg web page)
  - Add: SuplaDevice: add scheduleProtocolsRestart(timeMs) method
  - Add: SubdevicePairingHandler which is responsible for processing subdevice parining requests from server
  - Add: esp-idf: add setMaxTxPower to Wi-Fi class
  - Add: HVAC: add applyAdditionalValidation method for extra validations on HVAC derived classes (i.e. some MinTimeOffS limitations, etc.)
  - Add: TemperatureDropSensor, which tries to detect open window based on rapid temperature drop.
  - Add: HVAC: add HvacBase::stopCountDownTimer
  - Add: Html: add HomeScreenContentParameters to select what should be visible on device's screen/display via local cfg mode
  - Add: HVAC: add fixing readonly parameters in onLoadConfig and apply additional validations (if defined), add resetting channel config to defauls when funciton is set to "none"
  - Add: PowerCycle: add option to count power cycle only after power on reset
  - Add: RegisterDevice: add helper method to get "server compatible" text representation of device's GUID
  - Add: VirtualImpulseCounter: add handling of CALCFG reset counters.
  - Add: Supla::Sensor::OcrImpulseCounter - base class for impulse counter which sends photos to OCR server
  - Add: Supla::OrcIc - esp-idf implementation for Supla::Sensor::OcrImpulseCounter

## 24.06 (2024-06-03)

  - Fix: compilation fix for ESP32 boards for Arduino with version 3.x

## 24.05.02 (2024-05-31)

  - Change: default Supla proto version increased to 23
  - Change: registration message is using TDS_SuplaDeviceRegister_F and is send to server in chunks. This should reduce RAM memory usage on all boards.
  - Change: BH1750, MAX44009: set default GPM icon to 14 ("sun with light")
  - Add: Supla::Channel::setDefaultIcon(int) option that allow to configure initial default icon during registration of device.

## 24.05.01 (2024-05-22)

  - Fix: GPM: fix missing handling of "refresh interval ms" parameter
  - Fix: esp-idf ADC driver: deprecation warning fix for v5.2
  - Change: Storage: reduced minimum state save interval from 1 to 0.5 s
  - Change: esp-idf i2c driver: moved all i2c related classes to new i2c driver
  - Change: SuplaDevice: allowWorkInOfflineMode(2) will enable config mode for 1h if all protocols are not disabled.
  - Add: sd4linux: add support of MQTT client working as a gateway to Supla (thanks @Goral64)
  - Add: Clock: add option to disable remote device config flag setting
  - Add: StatusLed: add option to specify device's default status led setting
  - Add: HVAC: add initialConfig functionality which allows to define initial values of various HVAC parameters
  - Add: Relay: add Supla::Control::Relay::setRelayStorageSaveDelay(int delayMs) to modify default delay between state change and save to state storage
  - Add: ESP-IDF: add EspIdfSectorWlStorage class which use suplastorage partition and utilize wear leveling in "sector" mode
  - Add: sd4linux: Add a docker image for sd4linux (thanks @Goral64)
  - Add: sd4linux: Allow to use true/false in BinaryParsed
  - Add: StatusLed: add option LED_IN_CONFIG_MODE_ONLY - status led will blink only in config mode, otherwise it will be off. Additionally there is no device config for status led with this setting
  - Add: StatusLed: add option to disable device config usage
  - Add: HVAC: add getCountDownTimerEnds() method to get current countdown timer end timestamp
  - Add: DeviceConfig: add handling of ScreenDelayType
  - Add: HTML: add ScreenDelayTypeParameters form

## 24.03 (2024-03-14 Pi day)

  - Fix: MQTT: publish WiFi RSSI and signal strength only when those fields are available
  - Fix: Arduino esp32eth: code adjustment to changes in Network classes
  - Fix: GPM: fix for units longer than 14 bytes (last byte has to be null)
  - Change: sd4linux: update readme and airly_example.yaml with GPM classes descritpion
  - Change: sd4linux: change default proto version from 20 to 22
  - Change: esp-idf: supla-i2c-driver renamed to supla-i2c-deprecated, becasue esp-idf 5.2 introduced new format for i2c component. This should be removed when all components will be migrated to new driver
  - Change: Docker: update esp-idf to v5.2 in stable version
  - Add: Element, Channel: add setInitialCaption method which allow to define initial channel caption in Cloud and Apps
  - Add: Network: add option to define client builder in Network interface (createClient() method). Move network client initialization in supla srpc layer to later phase. By default supla srpc will use default network interface to build client instance.
  - Add: SuplaSrpc: add option to override client instance with setClient(Supla::Client\*) method
  - Add: Condition: add setThreshold method to update condition threshold. After calling setThreshold, condition is checked again and it will trigger when condition is met.
  - Add: Linux: add DistanceParsed channel
  - Add: esp-idf: add LAN8720 support
  - Add: Network: add obtaining of connection source IP address based on network client connection. Channel "state data" will be filled with source IP and then remaining state data is filled by network interface matching the source IP address. Add onLoadConfig to network classes.
  - Add: Linux: add IP address reporting in channel state.
  - Add: HTML: wifi parameters will show toggle for disabling this interface when more than one network interface is installed.
  - Add: HTML: add ethernet parameters to disable/enable ethernet interface.
  - Add: esp-idf: add SPI driver
  - Add: HVAC: add allowWrapAroundTemperatureSetpoints which enables wraping around on increase/decrease setpoint actions when they reach min and max setpoint value, i.e. setpoint 40 + 1 -> will set temeprature 5 (min)
  - Add: Esp-idf: add Wi-Fi connection fail return codes intruduced in esp-idf-v5.2
  - Add: SuplaDevice: add SuplaDevice.setProtoVerboseLog(true/false) to enable/disable Supla communiation logs
  - Add: ElectricityMeter: add support for vector balance parameters, add support for power active/reactive/apparent in kilo units
  - Add: MQTT: add filling IP address. Add publishing total fwd/rvr active energy vector balanced
  - Add: LocalAction, ActionHandlerClient: add methods to delete all actions related to trigger or client
  - Add: Config: add getter and setters for channel "cfg_chng" flag
  - Add: Html: add EmPhaseLed configuration for electricity meter phase indication LEDs.
  - Add: SelectInputParameter: add bool flag with information whether there was parameter change.
  - Add: Html: add EmCtTypeParameters for CT selection in EM
  - Add: Element: add setter for default channel function (setDefaultFunction)
  - Add: esp-idf: new version of supla-i2c-driver

## 24.02 (2024-02-14)

  - Fix: StatusLed: disable blinking for sleeping devices when other protocols are connected
  - Fix: WebServer: fix for invalid checkbox handling on beta web page
  - Fix: memory usage optimization
  - Change: Linux HVAC: "aux_thermometer_channel_no" is no longer mandatory
  - Change: SuplaDevice: default proto version increased to 21 (requires server version at least 23.11)
  - Change: LastStateLogger and HTML device info: add device uptime (seconds) information.
  - Change: HVAC: renamed "auto" mode and function to "heat_cool". WARNING: This may break compilation. Please rename all thermostat AUTO names to HEAT_COOL (in defines/consts) and "Auto" to "HeatCool" (in method names)
  - Change: StatusLed: disable status LED on soft reset state;
  - Add: GeneralPurposeMeasurement: add support
  - Add: GeneralPurposeMeter: add support
  - Add: LittleFsConfig: add option to define max buffer size for larger configs
  - Add: ExtPCA9685 - adding "setPWMFrequency" method (thanks lukfud)
  - Add: SuplaDevice: add hostname/wifi ssid generator support for "OH!" Supla devices. Improve method so it won't add "-" at the begining and it won't add duplicated "-".
  - Add: Button: add ActionHandler interface, add handling of ENABLE, TURN_ON, DISABLE, TURN_OFF, and TOGGLE actions which fully disables/enables the button
  - Add: Relay: add option to attach() multiple buttons.
  - Add: Relay: add storing and restoring configured function in Config storage
  - Add: HTML: add custom checkbox class (thanks lukfud)
  - Add: Storage: add wear leveling implementation for "byte access and write" memory types (like EEPROM, FRAM)
  - Add: Storage: add wear leveling implementation for "sector" mode (memory types like FLASH)
  - Add: Arduino IDE: add examples for GPM (KPOP & KLOP): GPM_BH1750_light_sensor, GP_Measurement, GPM_EspFreeHeap, GPM_MAX44009_light_sensor
  - Add: new sensors based on GPM added: BH1750, MAX44099, GpmEspFreeHeap
  - Add: HVAC: add events: ON_HVAC_WEEKLY_SCHEDULE_ENABLED, ON_HVAC_WEEKLY_SCHEDULE_ENABLED, ON_HVAC_WEEKLY_SCHEDULE_DISABLED, ON_HVAC_MODE_OFF, ON_HVAC_MODE_HEAT, ON_HVAC_MODE_COOL, ON_HVAC_MODE_HEAT_COOL

## 23.12 (2023-12-05)

  - Change: HVAC: allow invalid config with invalid main thermometer channel number.  Thermostat will be in "off" with theremometer error.
  - Change: RGBW, RGB, Dimmer: adjusted fading algorithm for smaller distances (target vs actuall PWM setting)
  - Change: MQTT: increased max username and password size to 255 chars.
  - Change: SuplaDevice: clear "LAST STATE" from detailed information about missing parameters in cofniguration in case of startup of fresh device with fully empty config. It will only have "Config mode" last state
  - Change: Button, ActionHandler: local actions which are "always enabled" are now not added to list of actions which are shown in Cloud as "disablesLocalOperation"
  - Change: SuplaDevice: enabled remote device config flag by default for proto >= 21
  - Change: ESP-IDF: version inrement to 5.1.2
  - Change: Adjusted isAnyUpdatePending method (used for sleeping devices) to runtime channel config support.
  - Change: SuplaDevice: change reporting "no connection to network" state from 10s to 20s timeout
  - Fix: HVAC: fix sending config after function being set to "none" and then to proper one
  - Fix: Linux: fix setting thermometer type for HVAC
  - Fix: CSS: restored label width 250px
  - Fix: HTML: fix for not concurent protocol selector HTML (Supla or MQTT)
  - Fix: ESP8266 RTOS example: update and adjustment to SDK changes. New default sdkconfig values.
  - Fix: Relay: fix manual turnOn command with no storage and with minimumAllowedDurationMs set
  - Fix: Clock::isReady fix fox DS1307RTC and DS3231RTC (thanks for @lukfud)
  - Fix: Uptime: calculation protection against invalid value in millis
  - Fix: Arduino ESP8266: SSL connection seems to have some memory leak and on each connection restart there is ~1 KB of memory less, which lead to quick OOM situation and board reset. Added Wi-Fi connection restart in case of lost SSL connection, which clears memory properly.
  - Fix: Arduino ESP8266: skip NTP time check for secured connection when Supla::Clock was already initialized earlier (thanks @lukfud)
  - Fix: HVAC: fix for multiple "weekly schedule save" problem on function change (thanks @lukfud)
  - Fix: HVAC: fix returning to manual mode during countdown timer in manual mode with different temperature setpoint
  - Fix: ESP-IDF: improved reconncetion procedure: add Wi-Fi connection retry on esp-idf level, add timeout for "got ip" event after wifi connection was established, for quicker reconnect, add ignoring first occurence of connection error (i.e. wifi scan may sometimes fail, so first occurence is ignored, but we report it when it repeats).
  - Fix: StatusLed: add mutex on access to interface methods in order to prevent potential error with led status change during runtime
  - Add: SuplaDevice: add setShowUptimeInChannelState(bool value) method to enable/disable uptime reporting by device (it may be disabled for deep sleep mode).
  - Add: PCA9685 expander support (thanks @lukfud)
  - Add: Hvac: add SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED flag
  - Add: ESP8266 RTOS: add support for local MQTT
  - Add: MQTT: add publishing of impulse relay functions (gate, door, garage door, gateway) as "always closed" with "open" button available for integration with step-by-step devices.
  - Add: MQTT: add support for binary sensor (with Home Assistant autodiscovery)
  - Add: RGBW, RGB, Dimmer: add option to specify PWM range. Add option to define Supla::Io interface for "leds" classes.
  - Add: Linux: add script for building the example and cmake improvements
  - Add: Relay: add minimum allowed duration ms parameter. If there will be turn on will lower (or 0) duration, then minumum value will be enforced.
  - Add: BinarySensor: add storing and obtaining function set on server
  - Add: Config: add method to provide default device config on device initialization and on factory reset
  - Add: Arduino ESP8266, ESP32: add logging SSL connection error to LAST STATE
  - Add: SuplaDevice: add method isRemoteDeviceConfigEnabled()
  - Add: ESP-IDF DS18B20: add sensor readout instant refresh after config change
  - Add: ESP-IDF: add bool guard for calling "disable" method, so it will be executed only when "start" was called before

## 23.11 (2023-11-07)

  - Change: LocalAction: changed variable that holds actions and events to uint16_t
  - Change: Button: Html fields now allow 200 ms as minimum time to be set (was 300 ms)
  - Change: Html: HVAC www reorganized
  - Change: HVAC: add reporting thermometer error when aux temperature protection is enabled, but thermometer reading is missing.
  - Change: Add handlig of "disabled" function from server (device will not try to send config for this channel to server anymore)
  - Fix: HVAC: change output behavior to respect min on/off time settings for anti-freeze/overheat protection and aux min/max setpoints.
  - Fix: HVAC: aux min/max setpoint should not override "off" mode
  - Fix: Relay: always send timer update to server on reconnect
  - Fix: HVAC: add earlier cleanup of HVAC flags for some scenarios
  - Fix: Relay: make sure that attached button was initialized before we read the state. It call onInit on button, so make sure that this method can be called multiple times.
  - Fix: RGBW, Dimmer: fix invalid value on analog write when turning off channel with configured max limit
  - Fix: RemoteDeviceConfig: fix handling of home screen off delay config from server
  - Fix: SRPC layer: add deinitialization of srpc and proto in order to cleanup data buffer on reconnect.
  - Fix: SuplaDevice: skip saving Storage and Config storage on soft reset request during reset to factory settings
  - Fix: ImpulseCounter: add initialization of previous GPIO state based on actual reading state from GPIO
  - Add: ActionTrigger: restored repeated "ON HOLD" action sending, however it has to be configured for >= 1000 ms. Otherwise AT for ON HOLD repeating is disabled.
  - Add: Html: add screen brightness HTML config
  - Add: Html CustomParameter: add default value
  - Add: MQTT: add support for Thermostat in MQTT and HA autodiscovery.
  - Add: ProtocolLayer: add config change notification
  - Add: SuplaDevice & Clock: add SUPLA_DEVICE_FLAG_CALCFG_SET_TIME support
  - Add: HVAC: add method to clear extra delay timestamps (i.e. after config change)
  - Add: Html: add H3 tag
  - Add: Html: RgbwButtonParameter - add option to specify custom field label
  - Add: RemoteDeviceConfig: Add "AjustmentForAutomatic" support for screen brightness. Adjusted HTML.
  - CSS: add class for input form range


## 23.10.01 (2023-10-17)

  - Change: HVAC: rename "screensaver" -> "home screen"
  - Fix: ESP-IDF MQTT: replace delay(0) with dedicated mutex in order to make sure that MQTT task will get CPU time ASAP
  - Fix: HVAC: fix for disabling anti-freeze mode when thermostat is off
  - Fix: BinaryBase: fix for handling channel config. Add handling of default invert state when config on server is missing
  - Fix: HVAC: add clearing of temperature in struct when value is missing from server
  - Fix: HVAC: anti-freeze function can be used only when heating mode is allowed. Overheat protection can be used only when cooling mode is allowed.
  - Fix: HVAC: add aux setpoint min/aux and antifreeze/overheat validation and runtime fix of invalid values
  - Fix: HVAC: init default temperature setpoint value after subfucntion change
  - Fix: Storage: invalidate storage when deleteAll was called
  - Add: HVAC: HVAC: add sending "timer state" extended channel value for timer function
  - Add: HVAC: add handling of AuxSetpointMinMaxEnabled
  - Add: HVAC: add button actions: TOGGLE_MANUAL_WEEKLY_SCHEDULE_MODES, TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES
  - Add: MQTT: add support for Dimmer, RGB, and RGBW channel with Home Assistant MQTT Autodiscovery
  - Add: RGBW, Dimmer, RGB: add support for commands (i.e. set only color, set brightness without turn on, etc.)
  - Add: RGBW, RGB, Dimmer: add actions for starting and stopping brightness iteration (by server). Add option to disable button local action from local config.
  - Add: GroupButtonControlRGBW: add class to handle group of RGBWs, RGBs, Dimmers by a single button

## 23.10 (2023-10-02)

  - Change: Correction: method "add" on temperature/humidity correction will modify existing correction if it was added earlier
  - Change: Arduino Config: moved blob structures to be stored in separate files in "/supla" directory
  - Change: HVAC: change how FuncList field is used (it stores only bits corresponding with supported functions instead of supported modes).
  - Change: ESP-IDF: bump esp-idf to 5.1.1
  - Change: HVAC: manually set temperature should be remembered and restored when manual mode is requested by user
  - Change: HVAC: change default temperature ranges per function
  - Fix: add fix for misspelled Supla server domain name svr->srv
  - Fix: SuplaSrpc: fix server ping problem when uptime is > 50 days
  - Fix: HVAC: multiple smaller bugfixes
  - Add: compilation flag SUPLA_EXCLUDE_LITTLEFS_CONFIG to exclude Supla LittleFs config implementation from compliation
  - Add: Linux: add HVAC channel integration
  - Add: Linux: add ability to specify proto version in yaml config file
  - Add: HVAC: add new algorithm "on/off at most" which enables heating when tempeature is below (setpoint - histeresis) and stops heating when setpoint is reached. This is default algorithm for DHW function. Other functions uses "on/off middle" algorithm
  - Add: Thermometer, ThermHygroMeter, HygroMeter: add ability to share correction configuration with server
  - Add: HVAC: add support for Domestic Hot Water function
  - Add: EnterCfgModeAfterPowerCycle: add class to trigger cfg mode after few quick power cycles
  - Add: BinarySensor: add handling of channel config
  - Add: HVAC: add handling of forced turn off of thremostat when i.e. window sensor detects open window
  - Add: HVAC: add "temperature setpoint change switch to manual mode" option for weekly schedule. Add weekly schedule manual override flag
  - Add: HVAC: add handling of CMD_SWITCH_TO_MANUAL_MODE
  - Add: BinaryBase: add handling of channel config finished
  - Add: HVAC: add option to control HVAC via addAction button integration
  - Add: SuplaDevice: add allowWorkInOfflineMode variant which requires all communication protocols to be disalbed in order to enable offline mode


## 23.08.01 (2023-08-17)

  - Change: ESP-IDF example: change partition scheme (factory removed, ota_0 and ota_1 size change to 1.5 M)
  - Change: Supla common update to proto v21
  - ESP-IDF SHT30: changed measurement period from 2s to 5s
  - Fix: ChannelElement: fix runAction (addAction is delegated to Channel, so runAction also should)
  - Fix: Clock: fix setting system time for esp32 targets
  - Fix: InternalPinOutput: add additional turnOn/Off during onInit after setting pinMode (in case of ESP32, GPIO state should be set after pinMode).
  - Fix: Arduino ESP8266: removed timezone setting from NTP request before creating ssl connection
  - Add HvacBase class for thermostats
  - Add VirtualImpulseCounter (to count from arbitary source). Add SecondsCounter (counts time of being enabled).
  - Add handling of "set device/channel config" for device and channel configuration sharign with server
  - Arduino IDE: add ThermostatBasic example
  - Linux cmake: add DOWNLOAD_EXTRACT_TIMESTAMP option
  - Proto, srpc: add methods for get/set device/channel config. Add weekly schedule get/set methods.
  - Linux compilation: add ccache
  - FreeRTOS: removed croutine.c from compilation (it was removed from FreeRTOS kernel)
  - Clock: add static getters for time
  - ESP-IDF SHT30: add initialization of values during onInit.
  - ESP-IDF: extracted i2c driver from sht30 (to share it with other devices on the same bus)
  - Button: add option to disable onLoadConfig
  - Add ThermometerDriver interface class for reading thermometer value from HW.
  - ESP-IDF: add ADC driver interface

## 23.07.01 (2023-07-20)

  - Change: default proto version for SuplaDevice changed from 16 to 20 (if you want to use other proto version, please specify it in SuplaDevice.begin(proto_version) call)
  - Change: supla-common update
  - Fix: Relay: fix for stackoverflow issue with ESP8266 and restoring countdown timer. Timer update message will be send on iterateConnected, instead of onRegistered callback.
  - Fix: Arduino IDE (ESP8266): for some reason when hostname is longer than 30 bytes, Wi-Fi connection can't be esablished. So as a workaround, we truncate hostname to 30 bytes
  - Add: abbility to send Push notifications directly from device
  - Add: Arduino IDE: add example with basic notifications usage (DSwithNotification)
  - Add: SuplaDevice: always print device's GUID in debug logs

## 23.07 (2023-07-07)

  - Change: Relay with Staircase function: duration of staircase timer is now fetched from server after registration. It is no longer required to turn on timer from Supla app in order to store a new timer value on device.
  - Change: Relay: keepTurnOnDuration() function is DEPRECATED. It can be removed from all application. Currently it is left empty in order to not break compilation of user applications.
  - Change: Impulse counter: restrict updates send to server to at most 1 Hz
  - Fix: Uptime: fix for uptime calculation error which resulted in very large number in "channel state info"
  - Fix: Electricity Meter: fixed clearing EM parameters in case of any problem with data reading. Only "energy" parameters will be send in case of problems and all other fields (including current) are cleared. Previous implmentation was leaving "current" field reported with previous value.
  - Fix: ESP-IDF: fix for reporting Wi-Fi connection fail reason
  - Add: ESP-IDF: ADC driver interface
  - Add: Relay: add countdowm timer support for Light and PowerSwitch functions. By default timer function is enabled and it will be updated on server after device firmware update. Countdown timer function can be disabled by calling disableCountdownTimerFunction() method. IMPORTANT: countdown timer fuction can't be removed from device without removing device from Cloud. WARNING: some custom implementation of classes inherit from Relay, may require adjustements to countdowm timer function.
  - Add: SuplaDevice: add info print in log with device name and SW version
  - Add: Element class: add onSoftReset callback
  - Add: ImpulseCounter: add support for counter reset from Cloud button


## 23.05 (2023-05-25)

  - Change: ESP-IDF version update to 5.0.2
  - Change: Config: rename of MQTT AT config parameter tag
  - Fix: Sleeping channel: channel value change should be always send after successful registration to Supla server (for sleeping channels) - fix for invalid binary sensor value for sleeping device.
  - Fix: RGBW, Dimmer: fix min range setting on "turn off". Min range should be adjusted only if bightness is > 0%, otherwise set 0 to HW.
  - Fix: RGBW: fix button toggle action - toggle with both RGB and W subchannels should turn off or turn on both subchannels, instead of toggleing both brightness and color brightness individually
  - Fix: Linux: fix CMake config for yaml-cpp lib, so it will work on MacOs also
  - Add: HTML: add SelectInputParameter for generic dropdown HTML select field with configurable tag, label and values
  - Add: RGBW, Dimmer: add option to specify brightness scaling function. Add smoother brighness change.
  - Add: Button: add detection of max configured multiclick value - button will send proper multiclick event without waiting for release or next press, when max configured click amount was reached (alignement with older framework behavior)
  - Add: Button: add events CONDITIONAL_ON_PRESS, CONDITIONAL_ON_RELEASE, CONDITIONAL_ON_CHANGE - those events are generated only on first press or release in sequence (i.e. only during first click, and ignored for other muliclicks. on release is also ignored after on hold).
  - Add: Button: add explicit setButtonType method for: monostable, bistable, motion sensor type setting.
  - Add: Button: add "motion sensor" button type
  - Add: HTML: add new HTML elements for button configuration (button type, hold time detection) and for RGBW setting (selection of which subchannel should be controlled by button).
  - Add: Relay, RGBWBase: add option to attach(button), which will be configured with default actions during onInit.
  - Add: HTML: add "use button as IN Config" HTML element
  - Add: Button: add option to automatically configure input as config based on Config Storage

## 23.04 (2023-04-19)

  - Change: (Arduino ESP32) Dimmer, RGB, RGBW: ESP32 LEDC channel frequency changed from 12 kHz to 1 kHz.
  - Change: ESP-IDF example: change partition scheme (factory removed, ota_0 and ota_1 size change to 1.5 M)
  - Change: change Supla communication send/recv log level to verbose.
  - Fix: RGBW: handling of command TurnOnOff value (server or app may send value 1, 2, 3 depending on source/channel type, which should be handled in the same way by device).
  - Fix: Arduino Mega ENC28J60: fix compilation error. Changed library for ENC28J60 to EthernetENC.
  - Add: sd4linux: add documentation for Humidity, Pressure, Rain, Wind parsed sensors.
  - Add: RollerShutter: add getters for closing/opening time

## 23.02.01 (2023-02-22)

  - Add: Roller shutter: add handling of server commands: up or stop, down or stop, step-by-step.
  - Change: Roller shutter: change local handling of "step by step" (i.e. by button) to use moveUp/Down instead of open/close.

## 23.02 (2023-02-20)

  - Add: Linux: add support for ActionTrigger for CmdRelay and BinaryParsed
  - Add: Distance sensor: add setReadIntervalMs method to set delay between sensor readouts (default 100 ms).
  - Add: Improved handlign of custom GPIO interface (i.e. for port expanders) for Relay, Binary, all buttons, all RollerShutter classes, BistableRelay, InternalPinOutput, PinStatusLed, StatusLed
  - Change: HC_SR04: changed delayMicroseconds 20 to 30 to make it work also wtih JSN-SR20-Y1 sensor
  - Fix: Linux: terminate SSL connection on critical SSL error
  - Fix: change logging from Serial to SUPLA_LOG_ macro for esp_wifi.h file (thanks @arturtadel)

## 22.12.01 (2023-01-09)

  - Fix: RGBW/Dimmer fix starting at lowest brighness when previously set brightness level was < 5%
  - Add: RGBW/Dimmer add option to set delay between dim direction change during dimming by button (setMinMaxIterationDelay)
  - Add: (ESP-IDF, ESP8266 RTOS) add HTTP status code to LAST STATE when it is different than 200

## 22.12 (2022-12-19)

  - Fix: Afore: fix crash on initialization
  - Fix: allow 32 bytes of Wi-Fi SSID
  - Fix: TrippleButtonRollerShutter add initialization of GPIO for STOP button
  - Fix: fixed reset to factory defaults for LittleFsConfig for Arduino IDE
  - Fix: Config mode: add escaping of HTML special characters when rendering user input values
  - Fix: (ESP-IDF, ESP8266 RTOS) always run full Wi-Fi scan even if we are performing reconnection procedure. Sometimes device fails in finding best AP during scan and without full scan option it connects to the first one found. Then device remembers it as last AP and adjacent connections will be performed to not optimal AP. In multi-AP Wi-Fi network this old behaviour can lead to stucking with connecting to distant APs instead of the one with the best signal.
  - Change: add CRC calculation and verification for Storage classes
  - Add: MQTT support for ElectricityMeter
  - Add: Linux support for ThermHygroMeterParsed
  - Add: getter for RollerShutter current direction
  - Add: RGBW, Dimmer: add events on turn on/off for each sub-channel
  - Add: Linux: add battery_level option
  - Add: Html: add CustomTextParameter for user defined text input in config mode
  - Add: Arduino: add example ConfigModeInputs
  - Add: Linux: add JSON parsing by path (JSON pointer)
  - Add: Linux: add example Airly integration
  - Add: Linux: add Afore integration
  - Add: Linux: add state storage file
  - Add: Linux: VirtualRelay - add option to define initial relay state (on/off/restore)
  - Add: Linux: add option to disable time expriation check for File source
  - Add: Linux: add CmdRelay - allows to execute Linux command on relay state change


## 22.11.03 (2022-11-28)

  - Fix: THW-01: enabled more detailed logging of connection problem in "LAST STATE" field
  - Fix: allow going back to "server disconnected" device state, when device is in error state
  - Add: (Arduino ESPx) add log with BSSID on connection to Wi-Fi establishment
  - Change: CALCFG handling - moved checking of "super user authorized" flag from supla_srpc to individual CALCFG handlers. Note: if your code use CALCFG then you should add checking if received CALCFG message has "authorized" flag set (if required by message).
  - Change: Storage: removed not used section types "device config" and "element config".
  - Add: Storage class: add "special storage section" functionality (see [4cf87f516c91](https://github.com/SUPLA/supla-device/commit/4cf87f516c91f2955f4b6eddd9069fa1cddc8112))
  - Add: ElectricityMeter: add handling of CALCFG reset counters message. Add EM channel flag informing about reset counters capability. It still requires implementation of resetStorage() method to work.
  - Add: ElectricityMeter: handling of Supla::RESET action
  - Add: TextCmdInputParameter HTML elements. It creates text input HTML field. You can register any text command that will trigger local action
when form is saved.
  - Add: SelectCmdInputParameter HTML element - works in the same way as TextCmdInputParameter, but generates select HTML input type with all available options


## 22.11.02 (2022-11-03)

  - Fix: "beta" config HTML page stuck at "Loading..."


## 22.11.01 (2022-11-02)

  - Change: THW-01 will try to connect for 15 s and then go to sleep in order to prevent internal heating.

## 22.10.05 (N/A)

  - Change: New CSS and HTML layout for web interface
  - Add: (Arduino ESPx, Arduino Mega) DS1307 external RTC support (thanks @lukfud)
  - Add: (ESP-IDF) add sending Action Trigger over MQTT
  - Add: ability for device to work in offline mode - allow normal functions without Wi-Fi, when Wi-Fi/server configuration is empty.


## 22.10.04 (2022-10-18)

  - Add: ActionTrigger support for publishing Home Assistant MQTT auto discovery
  - Add: Linux: support for new parsed sensors: HumidityParsed, PressureParsed, RainParsed, WindParsed
  - Fix: THW-01: Fixed random hang during encrypted connection establishment on private Supla servers.
  - Fix: Linux reading of uint8_t from yaml config should use int conversion instead of char (ASCII value)

## 22.10.03 (2022-10-12)

  - Change: Linux example extended with security_level setting
  - Add: support for factory test mode
  - Fix: watchdog timeout when using BME280 sensor or any other element with secondary channel

## 22.10.01 (2022-10-03)

  - Change: versioning format changed to year.month.number
  - Change: startup procedure and iterate methods adjusted to support concurrent multiprotocol scenarios (i.e. concurrent Supla and MQTT mode, MQTT only)
  - Change: device hostname/Wi-Fi AP name use 6 bytes of MAC address instead of 3
  - Change: ability to exit config mode depends on whether minimum configuration is provided
  - Change: logging improvement. All logs from supla-device use now SUPLA_LOG_ macros. It is recommended method to logging. It will optimize RAM usage (similar to F() macro in Arduino) and it is possible to remove all logs from binary file (please check log_wrapper.h) to reduce binary file size.
  - Change: LocalActions (like button handling) are now disabled when device enters config mode and it has minimum config fullfiled.
  - Change: (ESP-IDF) adjusted GPIO handling, so SW reset will not affect Relay's GPIO state
  - Change: (ESP-IDF) adjust OTA procedure for new updates server
  - Change: RGBW, Dimmer: dimming via button will change direction on each button press
  - Add: add SuplaDevice.setCustomHostnamePrefix method that override DHCP hostname and Wi-Fi SSID in CFG mode to user defined value (instead of using device name)
  - Add: support for conditional triggers in dimmers and RGB channels (thanks @lukfud)
  - Add: invert logic support for Binary sensor (thanks @lukfud)
  - Add: (Arduino ESPx, Arduino Mega) AHT sensor support (thanks @milanos)
  - Add: (Arduino ESPx, Arduino Mega) support for HX711 weight sensor (thanks @lukfud)
  - Add: (Arduino ESPx, Arduino Mega) DS3231 external RTC support (thanks @lukfud)
  - Add: (Arduino ESPx) add factory reset for configuration storage
  - Add: (ESP-IDF) SHT4x sensor support
  - Add: (ESP-IDF) SHT3x sensor support
  - Add: support for Supla root CA verification. Supla public servers cerficate is verified against Supla CA. Separate root CA is provided for verification of private Supla servers.
  - Add: ActionTrigger: add option to store list of activated actions from server in order to disable local action after power on reset (if it was disabled by server)
  - Add: ActionTrigger: option to decide if specific action can be disabled by server (i.e. we don't want to override enter to config mode)
  - Add: Button: multiclick time setting to HTML config
  - Add: Button: configureAsConfigButton method for simplified button configuration
  - Add: support for sending channel state info (i) for sleeping devices
  - Add: support for enter to config mode as device registration result for sleeping devices
  - Add: (ESP-IDF) support for MQTT for thermometers, thermometers with humidity, relays with Home Assistant MQTT autodiscovery.
  - Fix: selecting between raw and encrypted connection and between encyrpted with/without certficate verification.

## 2.4.2 (2022-06-20)

  - Change: (Arduino ESPx) Wi-Fi class handling change to support config mode
  - Change: StatusLed - change led sequence on error (300/100 ms)
  - Change: SuplaDevice removed entering to "No connection to network" state during first startup procedure for cleaner last status messages
  - Change: SuplaDevice renamed "Iterate fail" to "Communication fail" last state name.
  - Change: default device names adjustment
  - Change: Moved onInit methods for thermometers and therm-hygro meters to base classes.
  - Add: SuplaDevice: add handling of actions: enter config mode, toggle config mode, reset to factory default settings
  - Add: SuplaDevice: automatic GUID and AuthKey generation when it is missing and storage config is available. This functionality requires Storage Config class implementation of GUID/AuthKey generation on specific platform. By default it won't generate anything.
  - Add: RGBW, dimmer: add option to limit min/max values for brightness and colorBrightness
  - Add: SuplaDevice: ability to set user defined activity timeout
  - Add: VirtualThermometer
  - Add: VirtualThermHygroMeter.
  - Add: Element::onLoadConfig() method in which Element's config can be loaded
  - Add: set, clear, toggle methods for VirtualBinary
  - Add: SuplaDeviceClass::generateHostname method to generate DHCP hostname and AP Wi-Fi SSID in config mode
  - Add: support for SUPLA_CALCFG_CMD_ENTER_CFG_MODE (requires Storage::Config instance)
  - Add: Supla::Device::LastStateLogger to keep track of previous statuses and provide data for www
  - Add: WebServer class
  - Add: HTML blocks: DeviceInfo, ProtocolParameters, SwUpdate, SwUpdateBeta, WiFiParameters, CustomSwUpdate, StatusLedParameters
  - Add: (Arduino ESPx) LittleFsConfig class for configuration storage
  - Add: (ESP8266 RTOS, ESP IDF, Arduino ESPx, Linux): add GUID and AuthKey generation
  - Add: (ESP8266 RTOS, ESP IDF, Arduino ESPx): add web server for config mode
  - Add: (ESP8266 RTOS, ESP IDF) NvsConfig class for configuration storage
  - Add: (ESP8266 RTOS, ESP IDF) added detailed log to Last State for Wi-Fi connection problems and Supla server connection problem.
  - Add: (ESP8266 RTOS, ESP IDF) Sha256 and RsaVerificator implementation
  - Add: (ESP8266 RTOS, ESP IDF) OTA SW update procedure
  - Add: (Linux) time methods, timers implementation,
  - Add: (Linux) add ThermometerParsed, Supla::Source::Cmd, Supla::Parser::Simple, Supla::Parser::Json, ImpulseCouterParsed, ElectricityMeterParsed, Source::File, BinaryParsed
  - Add: (Linux) YAML config file support
  - Add: (Linux) file storage for last state log
  - Add: (Arduino ESPx) WebInterface Arduino example

## 2.4.1 (2022-03-23)

  - Change: (Arduino) move WiFi events for ESP8266 Arduino WiFi class to protected section
  - Change: (Arduino) Arduino ESP32 boards switch to version 2.x. Older boards will not compile ([see instructions](https://github.com/SUPLA/supla-device/commit/c533e73a4c811c026187374635dd812d4e294c8b))
  - Add: `Supla::Device::StatusLed` element (LED informing about currend device connection status)
  - Add: (Linux) support for compilation under Linux environment (partial implementation). Provided docker file for environment setup and example application.
  - Add: (FreeRTOS) support for compilation under FreeRTOS environment (partial implementation). Provided docker file for environment setup and example application.
  - Add: (ESP8266 RTOS) support for ESP8266 RTOS SDK envirotnment (partial implementation). Provided docker file for environment setup and example application.
  - Add: (ESP IDF) support for ESP IDF envirotnment (partial implementation). Provided docker file for environment setup and example application.
  - Add: `Supla::Io::pulseIn` and `Supla::Io::analogWrite` to interface
  - Add: (ESP8266 RTOS, ESP IDF) logging via ESP IDF logging lib
  - Add: (ESP8266 RTOS, ESP IDF) implementation for `delay`, `delayMicroseconds`, `millis`
  - Add: (ESP8266 RTOS, ESP IDF) implementation for `pinMode`, `digitalRead`, `digitalWrite`
  - Add: (ESP8266 RTOS, ESP IDF, Linux, FreeRTOS) `ChannelState` basic data reporting
  - Add: (ESP8266 RTOS, ESP IDF) `Supla::Storage` implementation on `spiffs`
  - Add: getters for electricity meter measured values
  - Add: PZEMv3 with custom PZEM address setting (allow to use single TX/RX pair for multiple PZEM units)

## 2.4.0 (2021-12-07)

All changes for this and older releases are for Arduino IDE target.

  - Add: Action Trigger support
  - Add: Conditions for Electricity meter
  - Add: MAX thermocouple
  - Change: Supla protocol version updated to 16
  - Fix: Roller shutter was sending invalid channel value for not calibrated state
  - Fix: Compilaton error and warnings cleanup (especially for ESP32)
