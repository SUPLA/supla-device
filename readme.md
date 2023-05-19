# SUPLA project

[SUPLA](https://www.supla.org) is an open source project for home automation. 

# SuplaDevice library
This repository contain SuplaDevice libraray. It allows to implement software for devices which connect to Supla infrastructure.

SuplaDevice can be used as library for Arduino IDE, or can be used directly with [ESP8266 RTOS SDK](https://github.com/espressif/ESP8266_RTOS_SDK) and with [ESP IDF](https://github.com/espressif/esp-idf). It is also possible to compile and run it on Linux and in FreeRTOS environment, however here many functions are still missing.

Please check our [changelog](CHANGELOG.md).

Below you can find basic infomration about SuplaDevice and instrutions for Arudino IDE. 

## Hardware requirements

### Arduino Mega
SuplaDevice works with Arduino Mega boards. Arduino Uno is not supported because of RAM limitations. It should work on other Arduino boards with at least 8 kB of RAM.
Following network interfaces are supported:
* Ethernet Shield with W5100 chipset
* ENC28J60 (not recommended - see Supported hardware section)

Warning: WiFi shields are currently not supported. 

### ESP8266 and ESP8285
ESP8266 and ESP8285 boards are supported. Network connection is done via WiFi.

### ESP32
ESP32 boards are supported with connection via WiFi or via Ethernet.

## SuplaDevice library installation for Arduino IDE

SuplaDevice is a library for [Arduino IDE](https://www.arduino.cc/en/main/software) that allows to implement devices working with [Supla](https://www.supla.org).

Please use library manager in Arduino IDE to install newest SuplaDevice library.

Before you start, you will need to:
1. install Arduino IDE,
2. install support for your board
3. install driver for your USB to serial converter device (it can be external device, or build in on your board)
4. make sure that communication over serial interface with your board is working (i.e. check some example Arduino application)
5. download and install this librarary

Above steps are standard Arudino IDE setup procedures not related to SuplaDevice library. You can find many tutorials on Internet with detailed instructions. Tutorials doesn't have to be related in any way with Supla. 

After step 5 you should see Supla example applications in Arduino IDE examples. Select one and have fun! Example file requires adjustments before you compile them and upload to your board. Please read all comments in example and make proper adjustments. 

## Usage

### Network interfaces
Supported network interfaces for Arduino Mega:
* Ethernet Shield - with W5100 chipset. Include `<supla/network/ethernet_shield.h>` and add `Supla::EthernetShield ethernet;` as a global variable.
* ENC28J60 - it requires additional EthernetENC library. Include `<supla/network/ENC28J60.h>` and 
add `Supla::ENC28J60 ethernet;` as a global variable. Warning: network initialization on this library is blocking. In case of missing ENC28J60 board
or some other problem with network, program will stuck on initialization and will not work until connection is properly esablished.
Second warning: UIPEthernet library is consuming few hundred of bytes of RAM memory more, compared to standard Ethernet library. 

Supported network interface for ESP8266:
* There is a native WiFi controller. Include `<supla/network/esp_wifi.h>` and add `Supla::ESPWifi wifi(ssid, password);` as a global variable and provide SSID and password in a constructor.
Warning: by default connection with Supla server is encrypted. Default settings of SSL consumes big amount of RAM. 
To disable SSL connection, use:
  `wifi.enableSSL(false);`



SSL certificate verification.
If you specify Supla's server certificate thumbprint there will be additional verification proceeded. Please use this method to configure fingerprint for validation:
  `wifi.setServersCertFingerprint("9ba818295ec60652f8221500e15288d7a611177");'



Supported network interface for ESP32:
* There is a native WiFi controller. Include `<supla/network/esp_wifi.h>` and add `Supla::ESPWifi wifi(ssid, password);` as a global variable and provide SSID and password in a constructor.

### Exmaples

Each example can run on Arduino Mega, ESP8266, or ESP32 board - unless mentioned otherwise in comments. Please read comments in example files and uncomment proper library for your network interface.

### Folder structure

* `supla-common` - Supla protocol definitions and structures. There are also methods to handle low level communication with Supla server, like message coding, decoding, sending and receiving. Those files are common with `supla-core` and the same code is run on multiple Supla platforms and services
* `supla/network` - implementation of network interfaces for supported boards
* `supla/sensor` - implementation of Supla sensor channels (thermometers, open/close sensors, etc.)
* `supla/storage` - implementation of persistant storage interfaces used by some Elements (i.e. keeping impulse counter data)
* `supla/control` - implementation of Supla control channels (various combinations of relays, buttons, action triggers)
* `supla/clock` - time services used in library (i.e. RTC)
* `supla/conditions` - classes that are used to check runtime dependencies between channels (i.e. turn on relay when humidity is below 40%)
* `supla/device` - device maintanance functions (like status LED which informs about connection status)
* `supla/pv` - supported integrations with inverters used with photovoltaic
* `supla/protocol` - protocol layers implementation for Supla and MQTT
* `supla` - all common classes are defined in main `supla` folder. You can find there classes that create framework on which all other components work. 

Some functions from above folders have dependencies to external libraries. Please check documentation included in header files.

### How does it work?

Everything that is visible in Supla (in Cloud, on API, mobile application) is called "channel". Supla channels are used to control relays, read temperature, check if garage door is open. 

SuplaDevice implements support for channels in `Channel` and `ChannelExtended` classes. Instances of those classes are part of objects called `Element` which are building blocks for any SuplaDevice application.

All sensors, relays, buttons objects inherits from `Element` class. Each instance of such object will automatically register in SuplaDevice and proper virtual methods will be called by SuplaDevice in a specified way.

All elements have to be constructed before `SuplaDevice.begin()` method is called.

Supla channel number is assigned to each elemement with channel in an order of creation of objects. First channel will get number 0, second 1, etc. Supla server will not accept registration of device when order of channels is changed, or some channel is removed. In such case, you should remove device from Supla Cloud and register it again from scratch.

`Element` class defines follwoing virtual methods that are called by SuplaDevice:
1. `onLoadConfig` - called first within `SuplaDevice.begin()` method. It reads element configuration from Supla::Config instance.
2. `onLoadState` - called second within `SuplaDevice.begin()` method. It reads element state data from persistent memory storage.
3. `onInit` - called third within `SuplaDevice.begin()` method. It should initialize your element - all GPIO settings should be done there and proper state of channel should be set. 
4. `onSaveState` - called in `SuplaDevice.iterate()` - it saves state data to persistant storage. It is not called on each iteration. `Storage` class makes sure that storing to memory does not happen too often and time delay between saves depends on implementation.
5. `onRegistered` - called in `SuplaDevice.iterate()` - it is called after SuplaDevice is registered to Supla server.
6. `iterateAlways` - called on each iteration of `SuplaDevice.iterate()` method, regardless of network/connection status. Be careful - some other methods called in `SuplaDevice.iterate()` method may block program execution for some time (even few seconds) - i.e. trying to establish connection with Supla server is blocking - in case server is not accessible, it will iterfere with `iterateAlways` method. So time critical functions should not be put here.
7. `iterateConnected` - called on each iterateion of `SuplaDevice.iterate()` method when device is connected and properly registered in Supla server. This method usually checks if there is some new data to be send to server (i.e. new temperature reading) and sends it. 
8. `onTimer` - called every 10 ms after enabling in `SuplaDevice.begin()`
9. `onFastTimer` - called every 1 ms (0.5 ms in case of Arudino Mega) after enabling in `SuplaDevice.begin()`

## How to migrate programs written in SuplaDevice libraray versions 1.6 and older

For Arduino Mega applications include proper network interface header:
```
// Choose proper network interface for your card:
// Arduino Mega with EthernetShield W5100:
#include <supla/network/ethernet_shield.h>
// Ethernet MAC address
uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
Supla::EthernetShield ethernet(mac);
//
// Arduino Mega with ENC28J60:
// #include <supla/network/ENC28J60.h>
// Supla::ENC28J60 ethernet(mac);

```

For ESP8266 based applications include wifi header and provide WIFI SSID and password:
```
// ESP8266 based board:
#include <supla/network/esp_wifi.h>
Supla::ESPWifi wifi("your_wifi_ssid", "your_wifi_password");
```

In case of ESP8266 remove all methods that defined network interface. They were usually added on the bottom of ino file, after end of loop() method, i.e.:
```
// Supla.org ethernet layer
    int supla_arduino_tcp_read(void *buf, int count) {
...
SuplaDeviceCallbacks supla_arduino_get_callbacks(void) {
...
```

Remove also those lines:
```
#define SUPLADEVICE_CPP
WiFiClient client;
```
You may also remove all WIFI related includes from ino file.

Common instruction for all boards:

If you use local IP address, please provide it in constructor of your network inteface class, i.e.:
```
Supla::EthernetShield ethernet(mac, localIp);
```

After that go to SuplaDevice.begin() method. Old code looked like this
```
SuplaDevice.begin(GUID,              // Global Unique Identifier 
                  mac,               // Ethernet MAC address
                  "svr1.supla.org",  // SUPLA server address
                  locationId,                 // Location ID 
                  locationPassword);          // Location Password
```
This method requires now different set of parameters:
```
  SuplaDevice.begin(GUID,              // Global Unique Identifier 
                    "svr1.supla.org",  // SUPLA server address
                    "mail@address.pl",     // Email address
                    AUTHKEY);          // Authentication key
```
What is different? Well, GUID and Supla server address it the same as previously. MAC address, location ID, location password are removed.
MAC address was moved to network interface class. Location ID and password were replaced with new authentication method - via email address
and authentication key. You can generate your authentication key in the same way as GUID (it is actually in exactly the same format):
```
// Generate AUTHKEY from https://www.supla.org/arduino/get-authkey
char AUTHKEY[SUPLA_AUTHKEY_SIZE] =  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
```

Next change is for custom digitalWrite and digitalRead methods. Those can be used to create virtual digital pins. Instead of adding custom
callback method that overrides digitalWrite/Read method, you should create a new class which inhertis from Supla::Io base class and define
your own customDigitalRead/Write methods. Here is short example (you can put this code in ino file, before setup()):
```
#include <supla/io.h>

class MyDigitalRead : public Supla::Io {
  public:
    int customDigitalRead(int channelNumber, uint8_t pin) {
      if (channelNumber == MY_EXTRA_VIRTUAL_CHANNEL) {
        return someCustomSourceOfData.getValue();
      } else {
        return ::digitalRead(pin);
      }
    }
}

MyDigitalRead instanceMyDigitalRead;
```

All channels from old version of library should be removed and created again in a new format. Please check instructions below how to add each type of channel.

## Supported channels
### Sensors
Sensor category is for all elements/channels that reads something and provides data to Supla server. All sensors are in `Supla::Sensor` namespace:

* `Binary` - two state sensor: on/off, enabled/disabled, open/closesd, etc. It reads GPIO state: LOW/HIGH
* `VirtualBinary` - similar to `Binary` but it use settable variable in memory to show state
* `VirtualThermometer` - thermometer channel with settable variable in memory
* `VirtualHygrometer` - humidity channel with settable variable in memory
* `VirtualThermHygroPressMeter` - thermometer, humidity, and pressure channel with settable variable in memory
* `Thermometer` - base class for thermometer sensors
* `DS18B20` - DS18B20 thermometer
* `Si7021` - S17021 thermometer
* `Si7021Sonoff` - Si7021 thermometer for Sonoff
* `MAX6675_K` - MAX6675_K thermometer
* `MAXThermocouple` - MAX6675 and MAX31855 thremocouples (thermometers)
* `NTC10k` - NTC10k thermometer
* `ThermHygroMeter` - base class for sensors capable of measuring temperature and humidity
* `DHT` - DHT11 and DHT22 support
* `SHT3x` - SHT3x support
* `AHT` - AHT temperature and humidity
* `ThermHygroPressMeter` - base class for sensors capable of measuring temperature, humidity, and pressure
* `BME280` - BME280 support
* `Distance` - base class for distance sensors
* `HC_SR04` - HC_SR04, JSN-SR20-Y1 distance meters
* `Pressure` - base class for presure meters
* `Wind` - base class for wind meters (speed)
* `Rain` - base class for rain meters
* `Weight` - base class for weight meters
* `ImpulseCounter` - calculates impulses on a given GPIO
* `ElectricityMeter` - base class for electricity meters
* `OnePhaseElectricityMeter` - base class for single phase electricity meters
* `PZEMv2` - PZEMv2 one phase electricity meter
* `PZEMv3` - PZEMv3 one phase electricity meter
* `ThreePhasePZEMv3` - 3x PZEMv3 for measuring three phases
* `EspFreeHeap` - provides free heap memory on ESP8266 as a Channel
* `HX711` - HX711 support (weight sensor)
* `HygroMeter` - base class for sensors capable of measuring humidity

### Control 
Control category is for all elements/channels that are used to control something, i.e. relays, buttons, RGBW.
Classes in this category are in namespace `Supla::Control`:

* `ActionTrigger` - allows to send Action Triggers to server based on events in application, can be attached to Button
* `BistableRelay` - SuplaDevice sends short impulses on GPIO to trigger change of bistable relay. It requires additional GPIO input to read status of relay
* `BistableRollerShutter` - Roller shutter implementation to control external roller shutter controllers that work in a similar way to bistable relays
* `Button` - allows to use button connected to GPIO to control other elements in device. Supports multiclicks, long click, etc
* `DimmerBase` - base class for dimmers
* `DimmerLeds` - PWM based implementation for dimmer
* `InternalPinOutput` - allows to control GPIO without showing it to Supla as a channel
* `LightRelay` - extension of Relay class that allows to monitor and configure lifespan of light source
* `PinStatusLed` - allows to duplicate GPIO state to another GPIO which can have connected LED to show status
* `Relay` - allows to control relay through GPIO
* `RGBBase` - base class for RGB control
* `RGBLeds` - PWM based implementation for RGB lights
* `RGBWBase` - base class for RGBW control
* `RGBWLeds` - PWM based implementation for RGBW lights
* `RollerShutter` - controller for roller shutters
* `SequenceButton` - extension of button which allows to trigger actions based on specific sequence/rythm
* `SimpleButton` - button that allows only press and release detection with lower memory footprint
* `TrippleButtonRollerShutter` - roller shutter implementation to control external roller shutter controllers with 3 buttons: up, down, stop
* `VirtualRelay` - relay which keeps its state in memory and doesn't affect any GPIO

### Photovoltaic inverter
SuplaDevice provides integrations for following inverters:

* `Afore`
* `Fronius`
* `SolarEdge`

## Supported persistant memory storage 
Storage class is used as an abstraction for different persistant memory devices. Some elements/channels will not work properly without storage and some will have limitted functionalities. I.e. `ImpulseCounter` requires storage to save counter value, so it could be restored after reset, or `RollerShutter` requires storage to keep openin/closing times and current shutter possition. Currently two variants of storage classes are supported.
### EEPROM/Flash memory
EEPROM (in case of Arduino Mega) and Flash (in case of ESP) are build into most of boards. Usually writing should be safe for 100 000 write operations (on each bit). So in order to limit those operations, this kind of Storage will save data in longer time periods (every few minutes). Supla will not write data if there is no change.
```
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom(SUPLA_STORAGE_OFFSET);
```
Offset parameter is optional - use it if you already use saving to EEPROM in your application and you want SuplaDevice to use some other area of memory.

### Adafruit FRAM SPI
FRAM is recommended for storage in Supla. It allows almost limitless writing cycles and it is very fast memory.
Currently only Adafruit FRAM SPI is supported.
```
#include <supla/storage/fram_spi.h>
// Hardware SPI
Supla::FramSpi fram(FRAM_CS, SUPLA_STORAGE_OFFSET);
```
or with SW SPI:
```
// Software SPI
Supla::FramSpi fram(SCK_PIN, MISO_PIN, MOSI_PIN, FRAM_CS, SUPLA_STORAGE_OFFSET);
```

## License


Please check it [here](https://github.com/SUPLA/supla-cloud/blob/master/LICENSE).


# Installation instructions for ESP IDF and ESP8266 RTOS SDK (draft)
## ESP32:
[Espressif ESP-IDF SDK installation steps for Linux (Ubuntu)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-setup.html) - run below commands:
```
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
mkdir ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
```

## ESP8266:
[Espressif RTOS-SDK ESP-IDF style](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html):
Assuming that ESP32 (ESP IDF) installation was already done, run those commands:
```
sudo apt install gcc git wget make libncurses-dev flex bison gperf python
cd ~/esp
wget https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz
tar -xzf xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz
git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git
/usr/bin/python -m pip install --user -r ~/esp/ESP8266_RTOS_SDK/requirements.txt
```

## ESP8266 and ESP32 environement setup:
Add to `~/.profile`:
```
# ESP tools setup options
export ESP_TOOLSET="none"
alias unload_esp='export ESP_TOOLSET="none" && export PATH=$(echo $PATH | sed -e "s#${HOME}/esp/[^:]*:##g" | sed -e "s#:${HOME}/esp/xtensa-lx106-elf/bin##g") && unset IDF_PATH && unset IDF_PYTHON_ENV_PATH && unset IDF_TOOLS_EXPORT_CMD && unset IDF_TOOLS_INSTALL_CMD && unset IDF_TOOLS_PATH'
alias use_esp8266='unload_esp && export PATH="$PATH:$HOME/esp/xtensa-lx106-elf/bin" && export ESP_TOOLSET="esp8266" && source ~/esp/ESP8266_RTOS_SDK/export.sh'
alias use_esp32='unload_esp && export IDF_TOOLS_PATH=~/esp/esp_idf_tools && export ESP_TOOLSET="esp32" && source ~/esp/esp-idf/export.sh'
```

This will provide `use_esp8266` and `use_esp32` commands on command line. If those commands are not available, then  your `.profile` file wasn't automatically loaded. You can load in manually by calling:
`source ~/.profile`

Commands `use_esp8266` and `use_esp32` will setup environment for those boards.

Last step is to setup `SUPLA_DEVICE_PATH` env variable. You can do it from command line:
```export SUPLA_DEVICE_PATH=/home/your_user_home_folder/path_to/supla-device/```
or you can add this line to `.profile` file so it will setup it automatically.

Now you can go to `extras/examples/` directory and select example for ESP8266 or for ESP32:

### ESP8266 example
Use `esp8266_rtos` directory (you can copy it to any location and modify). In this folder call:

`make menuconfig` and setup your build (especially select proper USB device for flashing). Then call:

`make build` - for building

`make flash` - for flashing ESP

`make monitor` - to run serial monitor (use CTRL+] to exit)

### ESP32 example
Use `esp_idf` directory (you can copy it to any location and modify). In this folder call:

`idf.py menuconfig` and setup your build (especially select proper USB device for flashing). Then call:

`idf.py build` - for building

`idf.py flash` - for flashing ESP

`idf.py monitor` - to run serial monitor (use CTRL+] to exit)

## FreeRTOS installation:
`sudo apt install libpcap-dev`
`git clone https://github.com/FreeRTOS/FreeRTOS.git --recurse-submodules`
