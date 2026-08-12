// SPDX-FileCopyrightText: malarz
// SPDX-License-Identifier: GPL-2.0-or-later

// Dependencies:
// https://github.com/arduino-libraries/Arduino_JSON

// It use ca. 20kB RAM, so ESP32 is highly recomended

#ifdef ARDUINO_ARCH_AVR
#error "aqi.eco is not supported on AVR"
#endif  // ARDUINO_ARCH_AVR

#ifndef SRC_SUPLA_PROTOCOL_AQI_ECO_H_
#define SRC_SUPLA_PROTOCOL_AQI_ECO_H_

// Allow insecure external TLS (not recommended)
// #define SUPLA_ALLOW_INSECURE_EXTERNAL_TLS

#include <supla/version.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <supla/network/client.h>
#include <supla/protocol/weathersender.h>

// Certificate for https://aqi.eco (LetsEncrypt)
// Valid until 2035-06-04
static const char LETS_ENCRYPT_CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

namespace Supla {
namespace Protocol {
class AQIECO : public Supla::Protocol::WeatherSender {
 public:
  explicit AQIECO(Supla::Network* _network, char token[], int refresh = 180,
    char server[] = "api.aqi.eco", int id = 0)
  : Supla::Protocol::WeatherSender(_network) {
    client = Supla::ClientBuilder();
    client->setSSLEnabled(true);
#if !defined(SUPLA_ALLOW_INSECURE_EXTERNAL_TLS)
    client->setCACert(LETS_ENCRYPT_CA_CERT);
#endif

    // serverAddress
    strncpy(serverAddress, server, 32);
    serverAddress[32] = 0;

    // apiToken
    if (strlen(token) == 32) {
      strncpy(apiToken, token, 32);
      apiToken[32] = 0;
    } else {
      apiToken[0] = 0;
    }
    // refreshTime
    if (refresh < 120) {
      refreshTime = 120;
    } else {
      refreshTime = refresh;
    }
    SUPLA_LOG_DEBUG("aqi.eco: refresh time: %d", refreshTime);

    // sensorId
    if (id == 0) {
      uint8_t mac[6] = {};
      _network->getMacAddr(mac);
      sensorId = ((mac[2]*256+mac[3])*256+mac[4])*256+mac[5];
    } else {
      sensorId = id;
    }
  }

  ~AQIECO() {
    delete client;
    client = nullptr;
  }

  bool sendData() override {
    if (strlen(apiToken) != 32) {
      SUPLA_LOG_DEBUG("aqi.eco: expected token length 32, got %d",
                      static_cast<int>(strlen(apiToken)));
      return false;
    }

    StaticJsonDocument<768> jsonBuffer;
    JsonObject json = jsonBuffer.to<JsonObject>();

    json["esp8266id"] = sensorId;
    json["software_version"] = "Supla_" SUPLA_SHORT_VERSION;
    JsonArray sensordatavalues = json.createNestedArray("sensordatavalues");

    for (int i=0; i < MAXSENSORS; i++) {
      if (sensors[i]) {
        double value = getSensorValue(i);
        String type = "unknown";
        switch (i) {
          case Supla::SenorType::PM1:
            type = "SPS30_P0";
            break;
          case Supla::SenorType::PM2_5:
            type = "SPS30_P2";
            break;
          case Supla::SenorType::PM4:
            type = "SPS30_P4";
            break;
          case Supla::SenorType::PM10:
            type = "SPS30_P1";
            break;
          case Supla::SenorType::TEMP:
            type = "BME280_temperature";
            break;
          case Supla::SenorType::HUMI:
            type = "BME280_humidity";
            break;
          case Supla::SenorType::PRESS:
            type = "BME280_pressure";
            value *= 100;
            break;
          case Supla::SenorType::LIGHT:
            type = "ambient_light";
            break;
          case Supla::SenorType::WIND:
            type = "wind_speed";
            break;
          case Supla::SenorType::RAIN:
            type = "rainfall";
            break;
          case Supla::SenorType::CO2:
            type = "conc_co2_ppm";
            break;
        }

        if (!isnan(value)) {
          JsonObject jo = sensordatavalues.createNestedObject();
          jo["value_type"] = type;
          jo["value"] = value;
        } else {
          return false;
        }
      }
    }
    char output[768];
    serializeJson(json, output, 768);
    SUPLA_LOG_DEBUG("aqi.eco: JSON: %s", output);

    if (client->connect(serverAddress, 443)) {
      client->print("POST /update/");
      client->print(apiToken);
      client->println(" HTTP/1.1");
      client->print("Host: ");
      client->println(serverAddress);
      client->println("Content-Type: application/json");
      client->print("Content-Length: ");
      client->println(strlen(output));
      client->println();
      client->println(output);

      SUPLA_LOG_DEBUG("aqi.eco: sended %d bytes to %s/update/%s",
        strlen(output), serverAddress, apiToken);

      // waiting for response
      delay(100);
      if (!client->available()) {
        SUPLA_LOG_DEBUG("aqi.eco: no bytes to read from %s", serverAddress);
        return false;
      }
      int responseLength = client->available();
      SUPLA_LOG_DEBUG("aqi.eco: reading from %s: %d bytes",
        serverAddress, responseLength);

      if (responseLength >= static_cast<int>(sizeof(output))) {
        responseLength = sizeof(output) - 1;
      }

      for (int i = 0; i < responseLength; i++) {
        int responseChar = client->read();
        if (responseChar < 0) {
          output[i] = 0;
          break;
        }
        output[i] = static_cast<char>(responseChar);
        if (output[i] == '\n') {
          output[i] = 0;
        }
      }
      output[responseLength] = 0;
      SUPLA_LOG_DEBUG("aqi.eco: response from %s: %s", serverAddress, output);
      return true;
    }
    return false;
  }

 private:
  ::Supla::Client *client = nullptr;
  char apiToken[33];
  char serverAddress[33];
  uint32_t sensorId = 0;
};
}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_AQI_ECO_H_
