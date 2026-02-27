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

#include <stdlib.h>
#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "fronius3p.h"
#include "supla/network/client.h"

namespace Supla {
namespace PV {

enum ParametersToRead { NONE, TOTAL_ENERGY, FAC, PAC, IAC_L1, IAC_L2, IAC_L3,
                        UAC_L1, UAC_L2, UAC_L3 };

Fronius3p::Fronius3p(IPAddress ip, int port, int deviceId)
    : ip(ip),
      port(port),
      buf(),
      totalGeneratedEnergy(0),
      currentPower(0),
      currentCurrent{},
      currentFreq(0),
      currentVoltage{},
      bytesCounter(0),
      retryCounter(0),
      valueToFetch(NONE),
      deviceId(deviceId),
      startCharFound(false),
      dataIsReady(false),
      dataFetchInProgress(false),
      connectionTimeoutMs(0),
      fetch3p(false),
      invDisabledCounter(0) {
  refreshRateSec = 15;
  client = Supla::ClientBuilder();
}

Fronius3p::~Fronius3p() {
  delete client;
  client = nullptr;
}

void Fronius3p::iterateAlways() {
  if (dataFetchInProgress) {
    if (millis() - connectionTimeoutMs > 30000) {
      SUPLA_LOG_DEBUG(
        "Fronius: connection timeout. Remote host is not responding");
      client->stop();
      dataFetchInProgress = false;
      dataIsReady = false;
      return;
    }
    if (!client->connected()) {
      SUPLA_LOG_DEBUG("Fronius fetch completed");
      dataFetchInProgress = false;
      dataIsReady = true;
    }
    if (client->available()) {
      SUPLA_LOG_DEBUG("Reading data from Fronius: %d", client->available());
    }
    while (client->available()) {
      char c;
      c = client->read();
      if (c == '\n') {
        if (startCharFound) {
          if (bytesCounter > 79) bytesCounter = 79;
          buf[bytesCounter] = '\0';
          char varName[80];
          char varValue[80];
          sscanf(buf, " %s  : %s", varName, varValue);
          if (valueToFetch != NONE &&
              strncmp(varName, "Value", strlen("Value")) == 0) {
            switch (valueToFetch) {
              case TOTAL_ENERGY: {
                float totalProd = atof(varValue);
                totalGeneratedEnergy = totalProd * 100;
                invDisabledCounter++;

                break;
              }
              case PAC: {
                float curPower = atof(varValue);
                currentPower = curPower * 100000;

                break;
              }
              case FAC: {
                float curFreq = atof(varValue);
                currentFreq = curFreq * 100;
                // assume that inverter is up if frequency is present
                invDisabledCounter = 0;

                break;
              }
              case IAC_L1: {
                float curCurrent = atof(varValue);
                currentCurrent[0] = curCurrent * 1000;

                break;
              }
              case IAC_L2: {
                float curCurrent = atof(varValue);
                currentCurrent[1] = curCurrent * 1000;

                break;
              }
              case IAC_L3: {
                float curCurrent = atof(varValue);
                currentCurrent[2] = curCurrent * 1000;

                break;
              }
              case UAC_L1: {
                float curVoltage = atof(varValue);
                currentVoltage[0] = curVoltage * 100;

                break;
              }
              case UAC_L2: {
                float curVoltage = atof(varValue);
                currentVoltage[1] = curVoltage * 100;

                break;
              }
              case UAC_L3: {
                float curVoltage = atof(varValue);
                currentVoltage[2] = curVoltage * 100;

                break;
              }
            }
            valueToFetch = NONE;
          } else if (strncmp(varName, "IAC_L1", strlen("IAC_L1")) == 0) {
            valueToFetch = IAC_L1;
          } else if (strncmp(varName, "IAC_L2", strlen("IAC_L2")) == 0) {
            valueToFetch = IAC_L2;
          } else if (strncmp(varName, "IAC_L3", strlen("IAC_L3")) == 0) {
            valueToFetch = IAC_L3;
          } else if (strncmp(varName, "UAC_L1", strlen("UAC_L1")) == 0) {
            valueToFetch = UAC_L1;
          } else if (strncmp(varName, "UAC_L2", strlen("UAC_L2")) == 0) {
            valueToFetch = UAC_L2;
          } else if (strncmp(varName, "UAC_L3", strlen("UAC_L3")) == 0) {
            valueToFetch = UAC_L3;
          } else if (strncmp(varName,
                             "TOTAL_ENERGY\"",
                             strlen("TOTAL_ENERGY")) == 0) {
            valueToFetch = TOTAL_ENERGY;
          } else if (strncmp(varName, "FAC", strlen("FAC")) == 0) {
            valueToFetch = FAC;
          } else if (strncmp(varName, "PAC", strlen("PAC")) == 0) {
            valueToFetch = PAC;
          }
        }
        bytesCounter = 0;
        startCharFound = false;
      } else if (c == '"' || startCharFound) {
        startCharFound = true;
        if (c == '"') {
          c = ' ';
        }
        if (bytesCounter < 80) {
          buf[bytesCounter] = c;
        }
        bytesCounter++;
      }
    }
    if (!client->connected()) {
      client->stop();
    }
  }
  if (dataIsReady) {
    dataIsReady = false;
    setFreq(currentFreq);
    setCurrent(0, currentCurrent[0]);
    setVoltage(0, currentVoltage[0]);
    /* divide by 3, because we do not have this data per
     phase, it is safe, cause inverter should always produce
     same amount of energy on each phase */
    setPowerActive(0, currentPower / 3);
    setFwdActEnergy(0, totalGeneratedEnergy / 3);
    setCurrent(1, currentCurrent[1]);
    setVoltage(1, currentVoltage[1]);
    setPowerActive(1, currentPower / 3);
    setFwdActEnergy(1, totalGeneratedEnergy / 3);
    setCurrent(2, currentCurrent[2]);
    setVoltage(2, currentVoltage[2]);
    setPowerActive(2, currentPower / 3);
    setFwdActEnergy(2, totalGeneratedEnergy / 3);
    updateChannelValues();

    /* workaround for missing data even if query succeeds:
     if 3 consecutive queries didn't returned frequency, assume
     that datamanager is up, but inverter is down so clear values */
    if (invDisabledCounter > 3) {
      SUPLA_LOG_DEBUG("Data is missing, inverter is down");
      invDisabledCounter = 4;  // overflow protection
      currentCurrent[0] = 0;
      currentVoltage[0] = 0;
      currentCurrent[1] = 0;
      currentVoltage[1] = 0;
      currentCurrent[2] = 0;
      currentVoltage[2] = 0;
      currentFreq = 0;
      currentPower = 0;
    }
  }
}

bool Fronius3p::iterateConnected() {
  if (!dataFetchInProgress) {
    if (lastReadTime == 0 || millis() - lastReadTime > refreshRateSec * 1000) {
      lastReadTime = millis();
      SUPLA_LOG_DEBUG("Fronius connecting %d", deviceId);
      if (client->connect(ip, port)) {
        retryCounter = 0;
        dataFetchInProgress = true;
        connectionTimeoutMs = lastReadTime;

        char buf[200];
        char idBuf[20];
        snprintf(idBuf, sizeof(idBuf), "%d", deviceId);
        strcpy(  // NOLINT(runtime/printf)
            buf,
            "GET "
            "/solar_api/v1/GetInverterRealtimeData.cgi?Scope=Device&DeviceID=");
        strcat(buf, idBuf);  // NOLINT(runtime/printf)
        if (fetch3p) {
          strcat(  // NOLINT(runtime/printf)
              buf,
              "&DataCollection=3PInverterData HTTP/1.1");
        } else {
          strcat(  // NOLINT(runtime/printf)
              buf,
              "&DataCollection=CommonInverterData HTTP/1.1");
        }
        SUPLA_LOG_VERBOSE("Fronius query: %s", buf);
        fetch3p = !fetch3p;
        client->println(buf);
        client->println("Host: localhost");
        client->println("Connection: close");
        client->println();

      } else {  // if connection wasn't successful, try few times. If it fails,
                // then assume that inverter is off during the night
        SUPLA_LOG_DEBUG("Failed to connect to Fronius");
        retryCounter++;
        if (retryCounter > 3) {
          currentPower = 0;
          currentFreq = 0;
          currentCurrent[0] = 0;
          currentVoltage[0] = 0;
          currentCurrent[1] = 0;
          currentVoltage[1] = 0;
          currentCurrent[2] = 0;
          currentVoltage[2] = 0;
          dataIsReady = true;
        }
      }
    }
  }
  return Element::iterateConnected();
}

void Fronius3p::readValuesFromDevice() {
}

}  // namespace PV
}  // namespace Supla
