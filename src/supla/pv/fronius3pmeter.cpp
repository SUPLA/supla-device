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
#include <stdio.h>

#include "fronius3pmeter.h"
#include "supla/network/client.h"

namespace Supla {
namespace PV {

enum ParametersToRead { NONE,
                        Current_AC_Phase_1,
                        Current_AC_Phase_2,
                        Current_AC_Phase_3,
                        EnergyReactive_VArAC_Sum_Consumed,
                        EnergyReactive_VArAC_Sum_Produced,
                        EnergyReal_WAC_Sum_Consumed,
                        EnergyReal_WAC_Sum_Produced,
                        Frequency_Phase_Average,
                        PowerApparent_S_Phase_1,
                        PowerApparent_S_Phase_2,
                        PowerApparent_S_Phase_3,
                        PowerFactor_Phase_1,
                        PowerFactor_Phase_2,
                        PowerFactor_Phase_3,
                        PowerReactive_Q_Phase_1,
                        PowerReactive_Q_Phase_2,
                        PowerReactive_Q_Phase_3,
                        PowerReal_P_Phase_1,
                        PowerReal_P_Phase_2,
                        PowerReal_P_Phase_3,
                        Voltage_AC_Phase_1,
                        Voltage_AC_Phase_2,
                        Voltage_AC_Phase_3 };

Fronius3pmeter::Fronius3pmeter(IPAddress ip, int port, int deviceId)
    : ip(ip),
      port(port),
      buf(),
      FwdReactEnergy(0),
      RvrReactEnergy(0),
      FwdActEnergy(0),
      RvrActEnergy(0),
      currentPower(0),
      currentActivePower{},
      currentApparentPower{},
      currentReactivePower{},
      currentPowerFactor{},
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
      connectionTimeoutMs(0) {
  refreshRateSec = 15;
  client = Supla::ClientBuilder();
}

Fronius3pmeter::~Fronius3pmeter() {
  delete client;
  client = nullptr;
}

void Fronius3pmeter::iterateAlways() {
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
          // remove last char if it is a comma
          if (buf[bytesCounter - 1] == ',') {
            buf[bytesCounter - 1] = '\0';
          } else {
            buf[bytesCounter] = '\0';
          }
          char varName[80];
          char varValue[80];
          sscanf(buf, " %s  : %s", varName, varValue);
          if (strncmp(
              varName, "Current_AC_Phase_1",
              strlen("Current_AC_Phase_1")) == 0) {
            valueToFetch = atof(varValue);
            currentCurrent[0] = valueToFetch * 1000;
          } else if (strncmp(
              varName, "Current_AC_Phase_2",
              strlen("Current_AC_Phase_2")) == 0) {
            valueToFetch = atof(varValue);
            currentCurrent[1] = valueToFetch * 1000;
          } else if (strncmp(
              varName, "Current_AC_Phase_3",
              strlen("Current_AC_Phase_3")) == 0) {
            valueToFetch = atof(varValue);
            currentCurrent[2] = valueToFetch * 1000;
          } else if (strncmp(
              varName, "EnergyReactive_VArAC_Sum_Consumed",
              strlen("EnergyReactive_VArAC_Sum_Consumed")) == 0) {
            valueToFetch = atof(varValue);
            FwdReactEnergy = valueToFetch * 100;
          } else if (strncmp(
              varName, "EnergyReactive_VArAC_Sum_Produced",
              strlen("EnergyReactive_VArAC_Sum_Produced")) == 0) {
            valueToFetch = atof(varValue);
            RvrReactEnergy = valueToFetch * 100;
          } else if (strncmp(
              varName, "EnergyReal_WAC_Sum_Consumed",
              strlen("EnergyReal_WAC_Sum_Consumed")) == 0) {
            valueToFetch = atof(varValue);
            FwdActEnergy = valueToFetch * 100;
          } else if (strncmp(
              varName, "EnergyReal_WAC_Sum_Produced",
              strlen("EnergyReal_WAC_Sum_Produced")) == 0) {
            valueToFetch = atof(varValue);
            RvrActEnergy = valueToFetch * 100;
          } else if (strncmp(
              varName, "Frequency_Phase_Average",
              strlen("Frequency_Phase_Average")) == 0) {
            valueToFetch = atof(varValue);
            currentFreq = valueToFetch * 100;
          } else if (strncmp(
              varName, "PowerApparent_S_Phase_1",
              strlen("PowerApparent_S_Phase_1")) == 0) {
            valueToFetch = atof(varValue);
            currentApparentPower[0] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "PowerApparent_S_Phase_2",
              strlen("PowerApparent_S_Phase_2")) == 0) {
            valueToFetch = atof(varValue);
            currentApparentPower[1] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "PowerApparent_S_Phase_3",
              strlen("PowerApparent_S_Phase_3")) == 0) {
            valueToFetch = atof(varValue);
            currentApparentPower[2] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "PowerFactor_Phase_1",
              strlen("PowerFactor_Phase_1")) == 0) {
            valueToFetch = atof(varValue);
            currentPowerFactor[0] = valueToFetch * 1000;
          } else if (strncmp(
              varName, "PowerFactor_Phase_2",
              strlen("PowerFactor_Phase_2")) == 0) {
            valueToFetch = atof(varValue);
            currentPowerFactor[1] = valueToFetch * 1000;
          } else if (strncmp(
              varName, "PowerFactor_Phase_3",
              strlen("PowerFactor_Phase_3")) == 0) {
            valueToFetch = atof(varValue);
            currentPowerFactor[2] = valueToFetch * 1000;
          } else if (strncmp(
              varName, "PowerReactive_Q_Phase_1",
              strlen("PowerReactive_Q_Phase_1")) == 0) {
            valueToFetch = atof(varValue);
            currentReactivePower[0] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "PowerReactive_Q_Phase_2",
              strlen("PowerReactive_Q_Phase_2")) == 0) {
            valueToFetch = atof(varValue);
            currentReactivePower[1] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "PowerReactive_Q_Phase_3",
              strlen("PowerReactive_Q_Phase_3")) == 0) {
            valueToFetch = atof(varValue);
            currentReactivePower[2] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "PowerReal_P_Phase_1",
              strlen("PowerReal_P_Phase_1")) == 0) {
            valueToFetch = atof(varValue);
            currentActivePower[0] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "PowerReal_P_Phase_2",
              strlen("PowerReal_P_Phase_2")) == 0) {
            valueToFetch = atof(varValue);
            currentActivePower[1] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "PowerReal_P_Phase_3",
              strlen("PowerReal_P_Phase_3")) == 0) {
            valueToFetch = atof(varValue);
            currentActivePower[2] = valueToFetch * 100000;
          } else if (strncmp(
              varName, "Voltage_AC_Phase_1",
              strlen("Voltage_AC_Phase_1")) == 0) {
            valueToFetch = atof(varValue);
            currentVoltage[0] = valueToFetch * 100;
          } else if (strncmp(
              varName, "Voltage_AC_Phase_2",
              strlen("Voltage_AC_Phase_2")) == 0) {
            valueToFetch = atof(varValue);
            currentVoltage[1] = valueToFetch * 100;
          } else if (strncmp(
              varName, "Voltage_AC_Phase_3",
              strlen("Voltage_AC_Phase_3")) == 0) {
            valueToFetch = atof(varValue);
            currentVoltage[2] = valueToFetch * 100;
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
    setFwdActEnergy(0, FwdActEnergy);
    setRvrActEnergy(0, RvrActEnergy);
    setFwdReactEnergy(0, FwdReactEnergy);
    setRvrReactEnergy(0, RvrReactEnergy);
    setVoltage(0, currentVoltage[0]);
    setVoltage(1, currentVoltage[1]);
    setVoltage(2, currentVoltage[2]);
    setCurrent(0, currentCurrent[0]);
    setCurrent(1, currentCurrent[1]);
    setCurrent(2, currentCurrent[2]);
    setFreq(currentFreq);
    setPowerActive(0, currentActivePower[0]);
    setPowerActive(1, currentActivePower[1]);
    setPowerActive(2, currentActivePower[2]);
    setPowerReactive(0, currentReactivePower[0]);
    setPowerReactive(1, currentReactivePower[1]);
    setPowerReactive(2, currentReactivePower[2]);
    setPowerApparent(0, currentApparentPower[0]);
    setPowerApparent(1, currentApparentPower[1]);
    setPowerApparent(2, currentApparentPower[2]);
    setPowerFactor(0, currentPowerFactor[0]);
    setPowerFactor(1, currentPowerFactor[1]);
    setPowerFactor(2, currentPowerFactor[2]);
    updateChannelValues();
  }
}

bool Fronius3pmeter::iterateConnected() {
  if (!dataFetchInProgress) {
    if (lastReadTime == 0 || millis() - lastReadTime > refreshRateSec * 1000) {
      lastReadTime = millis();
      SUPLA_LOG_DEBUG("Fronius connecting %d", deviceId);
      if (client->connect(ip, port)) {
        retryCounter = 0;
        dataFetchInProgress = true;
        connectionTimeoutMs = lastReadTime;

        char buf[100];
        strcpy(  // NOLINT(runtime/printf)
            buf,
            "GET "
            "/solar_api/v1/GetMeterRealtimeData.cgi?Scope=Device&DeviceId=");
        char idBuf[20];
        snprintf(idBuf, sizeof(idBuf), "%d", deviceId);
        strcat(buf, idBuf);        // NOLINT(runtime/printf)
        strcat(buf, " HTTP/1.1");  // NOLINT(runtime/printf)
        SUPLA_LOG_VERBOSE("Fronius query: %s", buf);
        client->println(buf);
        client->println("Host: localhost");
        client->println("Connection: close");
        client->println();

      } else {  // if connection wasn't successful, try few times. If it fails,
                // then assume that inverter is off during the night
        SUPLA_LOG_DEBUG("Failed to connect to Fronius");
        retryCounter++;
        if (retryCounter > 3) {
          FwdActEnergy = 0;
          RvrActEnergy = 0;
          FwdReactEnergy = 0;
          RvrReactEnergy = 0;
          currentVoltage[0] = 0;
          currentVoltage[1] = 0;
          currentVoltage[2] = 0;
          currentCurrent[0] = 0;
          currentCurrent[1] = 0;
          currentCurrent[2] = 0;
          currentFreq = 0;
          currentActivePower[0] = 0;
          currentActivePower[1] = 0;
          currentActivePower[2] = 0;
          currentReactivePower[0] = 0;
          currentReactivePower[1] = 0;
          currentReactivePower[2] = 0;
          currentApparentPower[0] = 0;
          currentApparentPower[1] = 0;
          currentApparentPower[2] = 0;
          currentPowerFactor[0] = 0;
          currentPowerFactor[1] = 0;
          currentPowerFactor[2] = 0;
          dataIsReady = true;
        }
      }
    }
  }
  return Element::iterateConnected();
}

void Fronius3pmeter::readValuesFromDevice() {
}

}  // namespace PV
}  // namespace Supla
