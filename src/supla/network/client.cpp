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

#include "client.h"

#include <supla/network/network.h>
#include <supla/supla_lib_config.h>
#include <string.h>
#include <supla/log_wrapper.h>
#include <SuplaDevice.h>
#include <stdio.h>

Supla::Client::Client() {
}

Supla::Client::~Client() {
  if (destroyCertOnExit && rootCACert != nullptr) {
    destroyCertOnExit = false;
    delete[] rootCACert;
    rootCACert = nullptr;
  }
}

int Supla::Client::connect(IPAddress ip, uint16_t port) {
  char server[100] = {};
  snprintf(server,
           99,
           "%d.%d.%d.%d",
           ip[0],
           ip[1],
           ip[2],
           ip[3]);
  return connect(server, port);
}

int Supla::Client::connect(const char *host, uint16_t port) {
  if (sslEnabled) {
    if (rootCACert == nullptr) {
      SUPLA_LOG_WARNING(
              "Connecting without certificate validation (INSECURE)");
    }
  }

  SUPLA_LOG_INFO(
            "Establishing %sencrypted connection with: %s (port: %d)",
            sslEnabled ? "" : "NOT ",
            host,
            port);

  return connectImp(host, port);
}

size_t Supla::Client::write(uint8_t data) {
  return write(&data, sizeof(data));
}

size_t Supla::Client::write(const uint8_t *buf, size_t size) {
#ifdef SUPLA_COMM_DEBUG
  if (debugLogs) {
    Supla::Network::printData("Send", buf, size);
  }
#endif
  return writeImp(buf, size);
}

size_t Supla::Client::println() {
  return println("");
}

size_t Supla::Client::println(const char *str) {
  int size = strlen(str);
  int response = 0;
  int dataSend = 0;
  if (size > 0) {
    response = write(reinterpret_cast<const uint8_t *>(str), size);
    if (response < 0) {
      return response;
    }
    dataSend += response;
  }
  response = write(reinterpret_cast<const uint8_t *>("\r\n"), 2);
  if (response <= 0) {
    return response;
  }
  dataSend += response;
  return dataSend;
}

size_t Supla::Client::print(const char *str) {
  int size = strlen(str);
  int response = 0;
  if (size > 0) {
    response = write(reinterpret_cast<const uint8_t *>(str), size);
    if (response < 0) {
      return response;
    }
  }

  return response;
}

void Supla::Client::setSSLEnabled(bool enabled) {
  sslEnabled = enabled;
}

void Supla::Client::setCACert(const char *rootCA, bool destroyCertOnExit) {
  if (rootCACert != nullptr && this->destroyCertOnExit) {
    delete[] rootCACert;
    rootCACert = nullptr;
  }

  this->destroyCertOnExit = destroyCertOnExit;
  rootCACert = rootCA;
}

int Supla::Client::read() {
  uint8_t result = 0;
  int response = read(&result, 1);

  if (response < 0) {
    return response;
  }

  if (response == 0) {
    return -1;
  }

  return result;
}

int Supla::Client::read(uint8_t *buf, size_t size) {
  int ret = readImp(buf, size);

  if (ret > 0) {
#ifdef SUPLA_COMM_DEBUG
    if (debugLogs) {
      Supla::Network::printData("Recv", buf, ret);
    }
#endif
  }

  return ret;
}

int Supla::Client::read(char *buf, size_t size) {
  return read(reinterpret_cast<uint8_t *>(buf), size);
}

void Supla::Client::setDebugLogs(bool debug) {
  debugLogs = debug;
}

void Supla::Client::setSdc(SuplaDeviceClass *newSdc) {
  sdc = newSdc;
}
