// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_NETWORKCLIENT_H_
#define EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_NETWORKCLIENT_H_

#include <Arduino.h>
#include <fake_lwip.h>
#include <string>
#include <vector>

class NetworkClient {
 public:
  inline static std::vector<std::string> events;
  inline static int nextFd = 12;
  inline static bool failConnect = false;
  inline static bool failWrite = false;
  inline static bool failRead = false;
  inline static bool failAvailable = false;
  inline static bool directCloseOnConnect = false;
  inline static NetworkClient *lastCreated = nullptr;

  NetworkClient() { lastCreated = this; }
  virtual ~NetworkClient() { events.push_back("socket-destroy"); }
  virtual int connect(const char *, uint16_t) {
    socketFd = nextFd;
    fakeLwipConnect();
    if (directCloseOnConnect) {
      events.push_back("raw-close");
      socketFd = -1;
      return 0;
    }
    if (failConnect) {
      stop();
      return 0;
    }
    return 1;
  }
  virtual void stop() {
    events.push_back("plain-stop");
    socketFd = -1;
  }
  virtual int available() {
    if (failAvailable) {
      stop();
      return 0;
    }
    return 1;
  }
  virtual int read(uint8_t *, size_t) {
    if (failRead) {
      stop();
      return -1;
    }
    return 1;
  }
  virtual size_t write(const uint8_t *, size_t size) {
    if (failWrite) {
      stop();
      return 0;
    }
    return size;
  }
  virtual uint8_t connected() { return socketFd >= 0; }
  virtual void setTimeout(uint16_t) {}
  virtual uint32_t localIP() { return 0; }
  int fd() const { return socketFd; }

 protected:
  int socketFd = -1;
};

#endif  // EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_NETWORKCLIENT_H_
