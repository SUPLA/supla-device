
// dependence: Arduino library for the Updated PZEM-004T v3.0 Power and Energy
// meter  https://github.com/mandulaj/PZEM-004T-v30

#include <SuplaDevice.h>
#include <supla/sensor/three_phase_PzemV3_ADDR.h>

// Choose proper network interface for your card:
#ifdef ARDUINO_ARCH_AVR
// Arduino Mega with EthernetShield W5100:
#include <supla/network/ethernet_shield.h>
// Ethernet MAC address
uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
Supla::EthernetShield ethernet(mac);

// Arduino Mega with ENC28J60:
// #include <supla/network/ENC28J60.h>
// Supla::ENC28J60 ethernet(mac);
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
// ESP8266 and ESP32 based board:
#include <supla/network/esp_wifi.h>
Supla::ESPWifi wifi("your_wifi_ssid", "your_wifi_password");
#endif

#define Pzem_RX 32
#define Pzem_TX 33

void setup() {

  Serial.begin(115200);

  // Replace the falowing GUID with value that you can retrieve from
  // https://www.supla.org/arduino/get-guid
  char GUID[SUPLA_GUID_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Replace the following AUTHKEY with value that you can retrieve from:
  // https://www.supla.org/arduino/get-authkey
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  /*
     Having your device already registered at cloud.supla.org,
     you want to change CHANNEL sequence or remove any of them,
     then you must also remove the device itself from cloud.supla.org.
     Otherwise you will get "Channel conflict!" error.
  */

  // ESP32  ThreePhasePZEMv3_ADDR(Serial prot, PZEM RX PIN, PZEM TX PIN, PZEM 1 Addr, PZEM 2 Addr, PZEM 3 Addr)
  new Supla::Sensor::ThreePhasePZEMv3_ADDR(&Serial2, Pzem_RX, Pzem_TX, 0xAA, 0xAB, 0xAC);

  // ESP32  ThreePhasePZEMv3_ADDR(Serial prot, PZEM RX PIN, PZEM TX PIN) "PZEM 1-3 Addr default to 0x01, 0x02, 0x03"
  // new Supla::Sensor::ThreePhasePZEMv3_ADDR(&Serial2, Pzem_RX, Pzem_TX);

  // ESP8266  ThreePhasePZEMv3_ADDR(PZEM RX PIN, PZEM TX PIN, PZEM 1 Addr, PZEM 2 Addr, PZEM 3 Addr)
  // new Supla::Sensor::ThreePhasePZEMv3_ADDR( Pzem_RX, Pzem_TX, 0xAA, 0xAB, 0xAC);

  // ESP8266  ThreePhasePZEMv3_ADDR(PZEM RX PIN, PZEM TX PIN) "PZEM 1-3 Addr default to 0x01, 0x02, 0x03"
  // new Supla::Sensor::ThreePhasePZEMv3_ADDR( Pzem_RX, Pzem_TX);

  /*
     SuplaDevice Initialization.
     Server address, is available at https://cloud.supla.org
     If you do not have an account, you can create it at
     https://cloud.supla.org/account/create SUPLA and SUPLA CLOUD are free of
     charge

  */

  SuplaDevice.begin(
    GUID,              // Global Unique Identifier
    "svr1.supla.org",  // SUPLA server address
    "email@address",   // Email address used to login to Supla Cloud
    AUTHKEY);          // Authorization key
}

void loop() {
  SuplaDevice.iterate();
}
