# CHANGELOG.md

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
