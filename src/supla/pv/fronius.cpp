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

#include "fronius.h"
#include "supla/network/client.h"

#define FRONIUS_SINGLE_PHASE_INVERTER 0
#define FRONIUS_THREE_PHASE_INVERTER 1
#define FRONIUS_THREE_PHASE_METER 2

namespace Supla {
namespace PV {

Fronius::Fronius(IPAddress ip, int port, int deviceId, int deviceType)
    : ip(ip),
      port(port),
      deviceType(deviceType),
      buf(),
      totalGeneratedEnergy(0),
      FwdReactEnergy(0),
      RvrReactEnergy(0),
      FwdActEnergy(0),
      RvrActEnergy(0),
      currentActivePower{},
      currentApparentPower{},
      currentReactivePower{},
      currentPowerFactor{},
      currentCurrent{},
      currentFreq(0),
      currentVoltage{},
      bytesCounter(0),
      retryCounter(0),
      deviceId(deviceId),
      startCharFound(false),
      dataIsReady(false),
      dataFetchInProgress(false),
      connectionTimeoutMs(0),
      variableToFetch(),
      fetch3p(false),
      invDisabledCounter(0) {
  if (deviceType == FRONIUS_SINGLE_PHASE_INVERTER) {
    extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED);
    extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED);
  }
  refreshRateSec = 15;
  client = Supla::ClientBuilder();
}

Fronius::~Fronius() {
  delete client;
  client = nullptr;
}

void Fronius::getSinglePhaseInverterValues(char* varName, char* varValue) {
  if (strncmp(
      varName, "TOTAL_ENERGY",
      strlen("TOTAL_ENERGY")) == 0) {
    totalGeneratedEnergy = atof(varValue) * 100;
    /* received data from inverter, so increment counter
     to check later if inverter is up and returned all
     data or is down and returned only total generated energy */
    invDisabledCounter++;
  } else if (strncmp(
      varName, "PAC",
      strlen("PAC")) == 0) {
    currentActivePower[0] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "FAC",
      strlen("FAC")) == 0) {
    currentFreq = atof(varValue) * 100;
    // assume that inverter is up if frequency is present
    invDisabledCounter = 0;
  } else if (strncmp(
      varName, "IAC",
      strlen("IAC")) == 0) {
    currentCurrent[0] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "UAC",
      strlen("UAC")) == 0) {
    currentVoltage[0] = atof(varValue) * 100;
  }
}

void Fronius::getThreePhaseInverterValues(char* varName, char* varValue) {
  if (strncmp(
      varName, "TOTAL_ENERGY",
      strlen("TOTAL_ENERGY")) == 0) {
    totalGeneratedEnergy = atof(varValue) * 100;
    /* received data from inverter, so increment counter
     to check later if inverter is up and returned all
     data or is down and returned only total generated energy */
    invDisabledCounter++;
  } else if (strncmp(
      varName, "PAC",
      strlen("PAC")) == 0) {
    currentActivePower[0] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "FAC",
      strlen("FAC")) == 0) {
    currentFreq = atof(varValue) * 100;
    // assume that inverter is up if frequency is present
    invDisabledCounter = 0;
  } else if (strncmp(
      varName, "IAC_L1",
      strlen("IAC_L1")) == 0) {
    currentCurrent[0] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "IAC_L2",
      strlen("IAC_L2")) == 0) {
    currentCurrent[1] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "IAC_L3",
      strlen("IAC_L3")) == 0) {
    currentCurrent[2] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "UAC_L1",
      strlen("UAC_L1")) == 0) {
    currentVoltage[0] = atof(varValue) * 100;
  } else if (strncmp(
      varName, "UAC_L2",
      strlen("UAC_L2")) == 0) {
    currentVoltage[1] = atof(varValue) * 100;
  } else if (strncmp(
      varName, "UAC_L3",
      strlen("UAC_L3")) == 0) {
    currentVoltage[2] = atof(varValue) * 100;
  }
}

void Fronius::getThreePhaseMeterValues(char* varName, char* varValue) {
  if (strncmp(
      varName, "Current_AC_Phase_1",
      strlen("Current_AC_Phase_1")) == 0) {
    currentCurrent[0] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "Current_AC_Phase_2",
      strlen("Current_AC_Phase_2")) == 0) {
    currentCurrent[1] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "Current_AC_Phase_3",
      strlen("Current_AC_Phase_3")) == 0) {
    currentCurrent[2] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "EnergyReactive_VArAC_Sum_Consumed",
      strlen("EnergyReactive_VArAC_Sum_Consumed")) == 0) {
    FwdReactEnergy = atof(varValue) * 100;
  } else if (strncmp(
      varName, "EnergyReactive_VArAC_Sum_Produced",
      strlen("EnergyReactive_VArAC_Sum_Produced")) == 0) {
    RvrReactEnergy = atof(varValue) * 100;
  } else if (strncmp(
      varName, "EnergyReal_WAC_Sum_Consumed",
      strlen("EnergyReal_WAC_Sum_Consumed")) == 0) {
    FwdActEnergy = atof(varValue) * 100;
  } else if (strncmp(
      varName, "EnergyReal_WAC_Sum_Produced",
      strlen("EnergyReal_WAC_Sum_Produced")) == 0) {
    RvrActEnergy = atof(varValue) * 100;
  } else if (strncmp(
      varName, "Frequency_Phase_Average",
      strlen("Frequency_Phase_Average")) == 0) {
    currentFreq = atof(varValue) * 100;
  } else if (strncmp(
      varName, "PowerApparent_S_Phase_1",
      strlen("PowerApparent_S_Phase_1")) == 0) {
    currentApparentPower[0] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "PowerApparent_S_Phase_2",
      strlen("PowerApparent_S_Phase_2")) == 0) {
    currentApparentPower[1] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "PowerApparent_S_Phase_3",
      strlen("PowerApparent_S_Phase_3")) == 0) {
    currentApparentPower[2] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "PowerFactor_Phase_1",
      strlen("PowerFactor_Phase_1")) == 0) {
    currentPowerFactor[0] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "PowerFactor_Phase_2",
      strlen("PowerFactor_Phase_2")) == 0) {
    currentPowerFactor[1] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "PowerFactor_Phase_3",
      strlen("PowerFactor_Phase_3")) == 0) {
    currentPowerFactor[2] = atof(varValue) * 1000;
  } else if (strncmp(
      varName, "PowerReactive_Q_Phase_1",
      strlen("PowerReactive_Q_Phase_1")) == 0) {
    currentReactivePower[0] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "PowerReactive_Q_Phase_2",
      strlen("PowerReactive_Q_Phase_2")) == 0) {
    currentReactivePower[1] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "PowerReactive_Q_Phase_3",
      strlen("PowerReactive_Q_Phase_3")) == 0) {
    currentReactivePower[2] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "PowerReal_P_Phase_1",
      strlen("PowerReal_P_Phase_1")) == 0) {
    currentActivePower[0] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "PowerReal_P_Phase_2",
      strlen("PowerReal_P_Phase_2")) == 0) {
    currentActivePower[1] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "PowerReal_P_Phase_3",
      strlen("PowerReal_P_Phase_3")) == 0) {
    currentActivePower[2] = atof(varValue) * 100000;
  } else if (strncmp(
      varName, "Voltage_AC_Phase_1",
      strlen("Voltage_AC_Phase_1")) == 0) {
    currentVoltage[0] = atof(varValue) * 100;
  } else if (strncmp(
      varName, "Voltage_AC_Phase_2",
      strlen("Voltage_AC_Phase_2")) == 0) {
    currentVoltage[1] = atof(varValue) * 100;
  } else if (strncmp(
      varName, "Voltage_AC_Phase_3",
      strlen("Voltage_AC_Phase_3")) == 0) {
    currentVoltage[2] = atof(varValue) * 100;
  }
}

void Fronius::setSinglePhaseInverterValues(bool zeroValues) {
  if (zeroValues) {
    currentActivePower[0] = 0;
    currentFreq = 0;
    currentCurrent[0] = 0;
    currentVoltage[0] = 0;
  } else {
    setFwdActEnergy(0, totalGeneratedEnergy);
    setPowerActive(0, currentActivePower[0]);
    setCurrent(0, currentCurrent[0]);
    setVoltage(0, currentVoltage[0]);
    setFreq(currentFreq);
  }
}

void Fronius::setThreePhaseInverterValues(bool zeroValues) {
  if (zeroValues) {
    currentCurrent[0] = 0;
    currentVoltage[0] = 0;
    currentCurrent[1] = 0;
    currentVoltage[1] = 0;
    currentCurrent[2] = 0;
    currentVoltage[2] = 0;
    currentFreq = 0;
    currentActivePower[0] = 0;
  } else {
    setFreq(currentFreq);
    setCurrent(0, currentCurrent[0]);
    setVoltage(0, currentVoltage[0]);
    /* divide by 3, because we do not have this data per
     phase, it is safe, cause inverter should always produce
     same amount of energy on each phase */
    setPowerActive(0, currentActivePower[0] / 3);
    setFwdActEnergy(0, totalGeneratedEnergy / 3);
    setCurrent(1, currentCurrent[1]);
    setVoltage(1, currentVoltage[1]);
    setPowerActive(1, currentActivePower[0] / 3);
    setFwdActEnergy(1, totalGeneratedEnergy / 3);
    setCurrent(2, currentCurrent[2]);
    setVoltage(2, currentVoltage[2]);
    setPowerActive(2, currentActivePower[0] / 3);
    setFwdActEnergy(2, totalGeneratedEnergy / 3);
  }
}

void Fronius::setThreePhaseMeterValues(bool zeroValues) {
  if (zeroValues) {
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
  } else {
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
  }
}

void Fronius::iterateAlways() {
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
          if (deviceType == FRONIUS_THREE_PHASE_METER) {
            // meter values are in same line so call directly
            Fronius::getThreePhaseMeterValues(varName, varValue);
          } else {
            // inverter values are in dict so we need to look for "Value"
            if (strncmp(varName, "Unit", strlen("Unit")) != 0) {
              if (strlen(variableToFetch) != 0 &&
                strncmp(varName, "Value", strlen("Value")) == 0) {
                if (deviceType == FRONIUS_SINGLE_PHASE_INVERTER) {
                  Fronius::getSinglePhaseInverterValues(
                    variableToFetch,
                    varValue);
                } else if (deviceType == FRONIUS_THREE_PHASE_INVERTER) {
                  Fronius::getThreePhaseInverterValues(
                    variableToFetch,
                    varValue);
                }
                variableToFetch[0] = 0;
              } else {
                snprintf(
                  variableToFetch,
                  sizeof(variableToFetch),
                  "%s",
                  varName);
              }
            }
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
    if (deviceType == FRONIUS_SINGLE_PHASE_INVERTER) {
      Fronius::setSinglePhaseInverterValues(false);
    } else if (deviceType == FRONIUS_THREE_PHASE_INVERTER) {
      Fronius::setThreePhaseInverterValues(false);
    } else if (deviceType == FRONIUS_THREE_PHASE_METER) {
      Fronius::setThreePhaseMeterValues(false);
    }
    updateChannelValues();

    /* workaround for missing data even if query succeeds:
     if 3 consecutive queries didn't returned frequency, assume
     that datamanager is up, but inverter is down so clear values,
     but only when fetching data for inverter */
    if (invDisabledCounter > 3) {
      SUPLA_LOG_DEBUG("Data is missing, inverter is down");
      invDisabledCounter = 4;  // overflow protection
      if (deviceType == FRONIUS_SINGLE_PHASE_INVERTER) {
        Fronius::setSinglePhaseInverterValues(true);
      } else if (deviceType == FRONIUS_THREE_PHASE_INVERTER) {
        Fronius::setThreePhaseInverterValues(true);
      /* for meter invDisabledCounter is not incremented so this
      code will never run, added for sake of closure */
      } else if (deviceType == FRONIUS_THREE_PHASE_METER) {
        Fronius::setThreePhaseMeterValues(true);
      }
    }
  }
}

void Fronius::getSinglePhaseInverterURL(char* buf, char* idBuf) {
  strcpy(  // NOLINT(runtime/printf)
      buf,
      "GET "
      "/solar_api/v1/GetInverterRealtimeData.cgi?Scope=Device&DeviceID=");
  strcat(buf, idBuf);  // NOLINT(runtime/printf)
  strcat(buf,          // NOLINT(runtime/printf)
      "&DataCollection=CommonInverterData HTTP/1.1");
}

void Fronius::getThreePhaseInverterURL(char* buf, char* idBuf) {
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
  fetch3p = !fetch3p;
}

void Fronius::getThreePhaseMeterURL(char* buf, char* idBuf) {
  strcpy(  // NOLINT(runtime/printf)
      buf,
      "GET "
      "/solar_api/v1/GetMeterRealtimeData.cgi?Scope=Device&DeviceId=");
  strcat(buf, idBuf);        // NOLINT(runtime/printf)
  strcat(buf, " HTTP/1.1");  // NOLINT(runtime/printf)
}

bool Fronius::iterateConnected() {
  if (!dataFetchInProgress) {
    if (lastReadTime == 0 || millis() - lastReadTime > refreshRateSec * 1000) {
      lastReadTime = millis();
      SUPLA_LOG_DEBUG("Fronius connecting %d", deviceId);
      if (client->connect(ip, port)) {
        retryCounter = 0;
        dataFetchInProgress = true;
        connectionTimeoutMs = lastReadTime;

        char idBuf[20];
        snprintf(idBuf, sizeof(idBuf), "%d", deviceId);
        char buf[200];
        if (deviceType == FRONIUS_SINGLE_PHASE_INVERTER) {
          Fronius::getSinglePhaseInverterURL(buf, idBuf);
        } else if (deviceType == FRONIUS_THREE_PHASE_INVERTER) {
          Fronius::getThreePhaseInverterURL(buf, idBuf);
        } else if (deviceType == FRONIUS_THREE_PHASE_METER) {
          Fronius::getThreePhaseMeterURL(buf, idBuf);
        }
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
          if (deviceType == FRONIUS_SINGLE_PHASE_INVERTER) {
            Fronius::setSinglePhaseInverterValues(true);
          } else if (deviceType == FRONIUS_THREE_PHASE_INVERTER) {
            Fronius::setThreePhaseInverterValues(true);
          } else if (deviceType == FRONIUS_THREE_PHASE_METER) {
            Fronius::setThreePhaseMeterValues(true);
          }
          dataIsReady = true;
        }
      }
    }
  }
  return Element::iterateConnected();
}

void Fronius::readValuesFromDevice() {
}

}  // namespace PV
}  // namespace Supla
