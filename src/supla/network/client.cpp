// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
}

Supla::ConnectionError Supla::Client::getConnectionError() const {
  return Supla::ConnectionError::NONE;
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
    if (!isCertificateValidationEnabled()) {
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

size_t Supla::Client::write(const void *buf, size_t size) {
  if (size == 0) {
    size = strnlen((const char *)buf, 1000);
  }
  if (size == 0) {
    return 0;
  }
  return write(reinterpret_cast<const uint8_t *>(buf), size);
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

#ifdef ARDUINO
size_t Supla::Client::println(const ::__FlashStringHelper *str) {
  return println(reinterpret_cast<const char *>(str));
}
#endif

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

#ifdef ARDUINO
size_t Supla::Client::print(const ::__FlashStringHelper *str) {
  return print(reinterpret_cast<const char *>(str));
}
#endif

void Supla::Client::setSSLEnabled(bool enabled) {
  sslEnabled = enabled;
}

void Supla::Client::setCACert(const char *rootCA) {
  rootCACert = rootCA;
}

bool Supla::Client::isCertificateValidationEnabled() const {
  return rootCACert != nullptr;
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

bool Supla::Client::isDebugLogs() const {
  return debugLogs;
}

void Supla::Client::setSdc(SuplaDeviceClass *newSdc) {
  sdc = newSdc;
}

uint32_t Supla::Client::getSrcConnectionIPAddress() const {
  // when 0 is returned, supla-device will use default network interface address
  return srcIp;
}
