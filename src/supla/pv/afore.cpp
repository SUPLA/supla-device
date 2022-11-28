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

#include "afore.h"

namespace Supla {
namespace PV {

Afore::Afore(IPAddress ip, int port, const char *loginAndPass)
    : ip(ip),
      port(port),
      buf(),
      totalGeneratedEnergy(0),
      currentPower(0),
      bytesCounter(0),
      retryCounter(0),
      vFound(false),
      varFound(false),
      dataIsReady(false),
      dataFetchInProgress(false),
      connectionTimeoutMs(0) {
  refreshRateSec = 15;
  int len = strlen(loginAndPass);
  if (len > LOGIN_AND_PASSOWORD_MAX_LENGTH) {
    len = LOGIN_AND_PASSOWORD_MAX_LENGTH;
  }
  strncpy(loginAndPassword, loginAndPass, len);
  client = Supla::ClientBuilder();
}

void Afore::iterateAlways() {
  if (dataFetchInProgress) {
    if (millis() - connectionTimeoutMs > 30000) {
      SUPLA_LOG_DEBUG(
                "AFORE: connection timeout. Remote host is not responding");
      client->stop();
      dataFetchInProgress = false;
      dataIsReady = false;
      return;
    }
    if (!client->connected()) {
      SUPLA_LOG_DEBUG("AFORE fetch completed");
      dataFetchInProgress = false;
      dataIsReady = true;
    }
    if (client->available()) {
      SUPLA_LOG_DEBUG("Reading data from afore: %d", client->available());
    }
    while (client->available()) {
      char c;
      c = client->read();
      if (c == '\n') {
        if (varFound) {
          if (bytesCounter > 79) bytesCounter = 79;
          buf[bytesCounter] = '\0';
          char varName[80];
          char varValue[80];
          sscanf(buf, "%s = \"%s\";", varName, varValue);
          if (strncmp(varName, "webdata_now_p", strlen("webdata_now_p")) == 0) {
            float curPower = atof(varValue);
            currentPower = curPower * 100000;
          }
          if (strncmp(varName, "webdata_total_e", strlen("webdata_total_e")) ==
              0) {
            float totalProd = atof(varValue);

            totalGeneratedEnergy = totalProd * 100000;
          }
        }
        bytesCounter = 0;
        vFound = false;
        varFound = false;
      } else if (c == 'v' || vFound) {
        vFound = true;
        if (bytesCounter < 80) {
          buf[bytesCounter] = c;
        }
        bytesCounter++;
        if (bytesCounter == 4 && !varFound) {
          if (strncmp(buf, "var ", 4) == 0) {
            varFound = true;
            bytesCounter = 0;
          }
        }
      }
    }
    if (!client->connected()) {
      client->stop();
    }
  }
  if (dataIsReady) {
    dataIsReady = false;
    setFwdActEnergy(0, totalGeneratedEnergy);
    setPowerActive(0, currentPower);
    updateChannelValues();
  }
}

bool Afore::iterateConnected() {
  if (!dataFetchInProgress) {
    if (lastReadTime == 0 || millis() - lastReadTime > refreshRateSec * 1000) {
      lastReadTime = millis();
      SUPLA_LOG_DEBUG("AFORE connecting");
      if (client->connect(ip, port)) {
        retryCounter = 0;
        dataFetchInProgress = true;
        connectionTimeoutMs = lastReadTime;

        client->print("GET /status.html HTTP/1.1\nAuthorization: Basic ");
        client->println(loginAndPassword);
        client->println("Connection: close");
        client->println();

      } else {  // if connection wasn't successful, try few times. If it fails,
                // then assume that inverter is off during the night
        SUPLA_LOG_DEBUG("Failed to connect to Afore");
        retryCounter++;
        if (retryCounter > 3) {
          currentPower = 0;
          dataIsReady = true;
        }
      }
    }
  }
  return Element::iterateConnected();
}

void Afore::readValuesFromDevice() {
}

}  // namespace PV
}  // namespace Supla
