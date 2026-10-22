// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_NETWORKCLIENTSECURE_H_
#define EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_NETWORKCLIENTSECURE_H_

#include <NetworkClient.h>
#include <memory>
#include <vector>

class NetworkClientSecure : public NetworkClient {
 protected:
  struct SslContext { int &socket; };
  // Match the real protected context and its final stop_ssl_socket deleter.
  std::shared_ptr<SslContext> sslclient{
      new SslContext{socketFd}, [](SslContext *context) {
        cleanup(context);
        delete context;
      }};

 public:
  inline static std::vector<int> closedSockets;
  ~NetworkClientSecure() override { events.push_back("tls-destroy"); }
  void stop() override {
    events.push_back("secure-stop");
    cleanup(sslclient.get());
  }
  void setCACert(const char *) {}
  void setInsecure() {}
  int lastError(char *, size_t) { return 0; }

 private:
  static void cleanup(SslContext *context) {
    if (context->socket >= 0) {
      closedSockets.push_back(context->socket);
    }
    // stop_ssl_socket() memsets the context in core 3.3.12.
    context->socket = 0;
  }
};

#endif  // EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_NETWORKCLIENTSECURE_H_
