// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "solaredge.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <supla/log_wrapper.h>
#include <supla/time.h>

// Allow insecure external TLS (not recommended)
// #define SUPLA_ALLOW_INSECURE_EXTERNAL_TLS

#define TEMPERATURE_NOT_AVAILABLE -275.0

#ifndef PROGMEM
#define PROGMEM
#endif

// Certificate for https://api.solaredge.com
// DigiCert Global Root CA
static const char SOLAREDGE_CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)EOF";

using Supla::PV::SolarEdge;

// This header should be received from server when data is returned
// It will be verified with actual header if they match. Otherwise, data
// structure is assumed to be changed and it will require this file to be
// adjusted to support new format
const char headerVerification[] =
    "date,inverterMode,temperature,totalActivePower,dcVoltage,"
    "groundFaultResistance,powerLimit,totalEnergy,vL1To2,vL2To3,vL3To1,L1-"
    "acCurrent,L1-acVoltage,L1-acFrequency,L1-apparentPower,L1-activePower,L1-"
    "reactivePower,L1-qRef,L1-cosPhi,L2-acCurrent,L2-acVoltage,L2-acFrequency,"
    "L2-apparentPower,L2-activePower,L2-reactivePower,L2-qRef,L2-cosPhi,L3-"
    "acCurrent,L3-acVoltage,L3-acFrequency,L3-apparentPower,L3-activePower,L3-"
    "reactivePower,L3-qRef,L3-cosPhi";

SolarEdge::SolarEdge(const char *apiKeyValue,
                     const char *siteIdValue,
                     const char *inverterSerialNumberValue,
                     Supla::Clock *clock)
    : clock(clock) {
  pvClient = Supla::ClientBuilder();
  pvClient->setSSLEnabled(true);
#if !defined(SUPLA_ALLOW_INSECURE_EXTERNAL_TLS)
  pvClient->setCACert(SOLAREDGE_CA_CERT);
#endif

  // SolarEdge api allows 300 requests daily, so it is one request per almost 5
  // min
  refreshRateSec = 6 * 60;  // refresh every 6 min

  int len = strlen(apiKeyValue);
  if (len > APIKEY_MAX_LENGTH) {
    len = APIKEY_MAX_LENGTH;
  }
  strncpy(apiKey, apiKeyValue, len);

  len = strlen(siteIdValue);
  if (len > PARAMETER_MAX_LENGTH) {
    len = PARAMETER_MAX_LENGTH;
  }
  strncpy(siteId, siteIdValue, len);

  len = strlen(inverterSerialNumberValue);
  if (len > PARAMETER_MAX_LENGTH) {
    len = PARAMETER_MAX_LENGTH;
  }
  strncpy(inverterSerialNumber, inverterSerialNumberValue, len);

  temperatureChannel.setType(SUPLA_CHANNELTYPE_THERMOMETER);
  temperatureChannel.setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
  temperatureChannel.setNewValue(TEMPERATURE_NOT_AVAILABLE);
}

SolarEdge::~SolarEdge() {
  delete pvClient;
  pvClient = nullptr;
}

void SolarEdge::iterateAlways() {
  if (dataFetchInProgress) {
    if (millis() - connectionTimeoutMs > 30000) {
      SUPLA_LOG_DEBUG(
          "SolarEdge: connection timeout. Remote host is not responding");
      pvClient->stop();
      dataFetchInProgress = false;
      dataIsReady = false;
      return;
    }
    if (!pvClient->connected()) {
      SUPLA_LOG_DEBUG("SolarEdge fetch completed");
      dataFetchInProgress = false;
      dataIsReady = true;
    }
    if (pvClient->available()) {
      SUPLA_LOG_DEBUG("Reading data from SolarEdge: %d", pvClient->available());
    }
    while (pvClient->available()) {
      int readResult = pvClient->read();
      if (readResult < 0) {
        break;
      }
      char c = static_cast<char>(readResult);
      if (c == '\n') {
        if (bytesCounter > 0) {
          // new line is found with bytesCounter > 0 means that we have full
          // received line of data in buf
          buf[bytesCounter] =
              '\0';  // add null character at the end of received string
          SUPLA_LOG_VERBOSE("Received line: %s", buf);
          if (!headerFound) {
            if (0 == strncmp(headerVerification,
                             buf,
                             sizeof(headerVerification) - 1)) {
              headerFound = true;
            }
          } else {
            int commaCount = 0;
            for (unsigned int i = 0; i < strlen(buf); i++) {
              if (buf[i] == ',') commaCount++;
            }
            // proper line of data should contain at least 34 commas
            if (commaCount >= 34) {
              strtok(buf, ",");
              for (int i = 1; i < 34; i++) {
                char *value =
                    strtok(nullptr, ",");  // NOLINT(runtime/threadsafe_fn)
                /*
0 date,
1 inverterMode,
2 temperature,
3 totalActivePower,
4 dcVoltage,
5 groundFaultResistance,
6 powerLimit,
7 totalEnergy,
8 vL1To2,
9 vL2To3,
10 vL3To1,
11 L1-acCurrent,
12 L1-acVoltage,
13 L1-acFrequency,
14 L1-apparentPower,
15 L1-activePower,
16 L1-reactivePower,
17 L1-qRef,
18 L1-cosPhi,
19 L2-acCurrent,
20 L2-acVoltage,
21 L2-acFrequency,
22 L2-apparentPower,
23 L2-activePower,
24 L2-reactivePower,
25 L2-qRef,
26 L2-cosPhi,
27 L3-acCurrent,
28 L3-acVoltage,
29 L3-acFrequency,
30 L3-apparentPower,
31 L3-activePower,
32 L3-reactivePower,
33 L3-qRef,
34 L3-cosPhi
*/
                switch (i) {
                  case 1: {  // inverterMode
                    if (strncmp(value, "MPPT", 4) != 0) {
                      // ignoring data for inverter in mode other than MPPT
                      i = commaCount;
                    }
                    break;
                  }
                  case 2: {  // temperature
                    temperature = atof(value);
                    break;
                  }
                  case 7: {  // totalEnergy - split per 3 phases
                    double energy = atof(value);
                    totalGeneratedEnergy = energy * 100;
                    break;
                  }
                  case 11: {  // L1 - acCurrent
                    double current = atof(value);
                    currentCurrent[0] = current * 1000;
                    break;
                  }
                  case 12: {  // L1 - acVoltage
                    double voltage = atof(value);
                    currentVoltage[0] = voltage * 100;
                    break;
                  }
                  case 13: {  // L1 - acFrequency
                    double frequency = atof(value);
                    currentFreq = frequency * 100;
                    break;
                  }
                  case 14: {  // L1 - apparentPower
                    double power = atof(value);
                    currentApparentPower[0] = power * 100000;
                    break;
                  }
                  case 15: {  // L1 - activePower
                    double power = atof(value);
                    currentActivePower[0] = power * 100000;
                    break;
                  }
                  case 16: {  // L1 - ReactivePower
                    double power = atof(value);
                    currentReactivePower[0] = power * 100000;
                    break;
                  }
                  case 19: {                       // L2 - acCurrent
                    double current = atof(value);  // Wh
                    currentCurrent[1] = current * 1000;
                    break;
                  }
                  case 20: {                       // L2 - acVoltage
                    double voltage = atof(value);  // Wh
                    currentVoltage[1] = voltage * 100;
                    break;
                  }
                  case 22: {  // L2 - apparentPower
                    double power = atof(value);
                    currentApparentPower[1] = power * 100000;
                    break;
                  }
                  case 23: {  // L2 - activePower
                    double power = atof(value);
                    currentActivePower[1] = power * 100000;
                    break;
                  }
                  case 24: {  // L2 - ReactivePower
                    double power = atof(value);
                    currentReactivePower[1] = power * 100000;
                    break;
                  }
                  case 27: {                       // L3 - acCurrent
                    double current = atof(value);  // Wh
                    currentCurrent[2] = current * 1000;
                    break;
                  }
                  case 28: {                       // L3 - acVoltage
                    double voltage = atof(value);  // Wh
                    currentVoltage[2] = voltage * 100;
                    break;
                  }
                  case 30: {  // L3 - apparentPower
                    double power = atof(value);
                    currentApparentPower[2] = power * 100000;
                    break;
                  }
                  case 31: {  // L3 - activePower
                    double power = atof(value);
                    currentActivePower[2] = power * 100000;
                    break;
                  }
                  case 32: {  // L3 - ReactivePower
                    double power = atof(value);
                    currentReactivePower[2] = power * 100000;
                    break;
                  }
                    // acCurrent setCurrent
                    // acVoltage
                    // acFreq
                    // apparentPower
                    // activePower
                    // ReactivePower
                }
              }
            }
          }
        }
        bytesCounter = 0;
      } else if (bytesCounter < 1023) {
        buf[bytesCounter] = c;
        bytesCounter++;
      }
    }
    if (!pvClient->connected()) {
      pvClient->stop();
    }
  }
  if (dataIsReady) {
    dataIsReady = false;
    headerFound = false;
    for (int i = 0; i < 3; i++) {
      if (totalGeneratedEnergy > 0) {
        setFwdActEnergy(i, totalGeneratedEnergy / 3.0);
      }
      setPowerActive(i, currentActivePower[i]);
      currentActivePower[i] = 0;
      setCurrent(i, currentCurrent[i]);
      currentCurrent[i] = 0;
      setVoltage(i, currentVoltage[i]);
      currentVoltage[i] = 0;
      setPowerApparent(i, currentApparentPower[i]);
      currentApparentPower[i] = 0;
      setPowerReactive(i, currentReactivePower[i]);
      currentReactivePower[i] = 0;
    }
    totalGeneratedEnergy = 0;
    setFreq(1);
    setFreq(currentFreq);
    currentFreq = 0;
    temperatureChannel.setNewValue(temperature);
    temperature = TEMPERATURE_NOT_AVAILABLE;
    updateChannelValues();
  }
}

bool SolarEdge::iterateConnected() {
  if (clock && clock->isReady()) {
    if (!dataFetchInProgress) {
      bytesCounter = 0;
      if (lastReadTime == 0 ||
          millis() - lastReadTime >
              (retryCounter > 0 ? 5000 : refreshRateSec * 1000)) {
        lastReadTime = millis();
        SUPLA_LOG_DEBUG("SolarEdge connecting");
        int returnCode = pvClient->connect("monitoringapi.solaredge.com", 443);
        if (returnCode) {
          retryCounter = 0;
          dataFetchInProgress = true;
          connectionTimeoutMs = lastReadTime;
          SUPLA_LOG_DEBUG("Succesful connect");

          time_t timestamp = time(0);  // get current time
          timestamp -= 10 * 60;        // go back in time 10 minutes

#define SOLAR_TMP_BUFFER_SIZE 100

          char startTime[SOLAR_TMP_BUFFER_SIZE];
          char endTime[SOLAR_TMP_BUFFER_SIZE];

          struct tm timeinfo;
          gmtime_r(&timestamp, &timeinfo);

          snprintf(startTime,
                   SOLAR_TMP_BUFFER_SIZE,
                   "%d-%d-%d%%20%d:%d:%d",
                   timeinfo.tm_year + 1900,
                   timeinfo.tm_mon + 1,
                   timeinfo.tm_mday,
                   timeinfo.tm_hour,
                   timeinfo.tm_min,
                   timeinfo.tm_sec);
          snprintf(endTime,
                   SOLAR_TMP_BUFFER_SIZE,
                   "%d-%d-%d%%2023:59:59",
                   timeinfo.tm_year + 1900,
                   timeinfo.tm_mon + 1,
                   timeinfo.tm_mday);

          char query[512];
          int queryLen = snprintf(query,
                                  sizeof(query),
                                  "GET /equipment/%s/%s/data.csv?startTime=%s"
                                  "&endTime=%s&api_key=%s HTTP/1.1",
                                  siteId,
                                  inverterSerialNumber,
                                  startTime,
                                  endTime,
                                  apiKey);
          if (queryLen < 0 || queryLen >= static_cast<int>(sizeof(query))) {
            SUPLA_LOG_ERROR("SolarEdge query buffer overflow");
            pvClient->stop();
            dataFetchInProgress = false;
            return Element::iterateConnected();
          }

          SUPLA_LOG_VERBOSE("SolarEdge query: %s", query);
          pvClient->println(query);
          pvClient->println(F("Host: localhost"));
          pvClient->println(F("Connection: close"));
          pvClient->println();

        } else {  // if connection wasn't successful, try few times
          SUPLA_LOG_DEBUG("Failed to connect to SolarEdge api, return code: %d",
                          returnCode);
          retryCounter++;
        }
      }
    }
  }
  return Element::iterateConnected();
}

void SolarEdge::readValuesFromDevice() {
}

Supla::Channel *SolarEdge::getSecondaryChannel() {
  return &temperatureChannel;
}

#endif
