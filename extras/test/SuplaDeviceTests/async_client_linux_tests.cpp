// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

// This file is built by the separate real-socket transport target, not the
// SDK test double target below.
#ifdef SUPLA_ASYNC_LINUX_TEST
#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <supla/network/network.h>
#include <chrono>
#include <thread>
#include <string>
#include "../../porting/linux/linux_client.h"

void Supla::Network::printData(const char *, const void *, int) {}

uint32_t millis() {
  using Clock = std::chrono::steady_clock;
  static const auto start = Clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      Clock::now() - start).count();
}
void delay(uint64_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

namespace {
using Result = Supla::Client::ConnectResult;
struct Listener {
  int fd = -1;
  uint16_t port = 0;
  Listener() {
    signal(SIGPIPE, SIG_IGN);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (fd < 0 || bind(fd, reinterpret_cast<sockaddr *>(&address),
                       sizeof(address)) || listen(fd, 8)) return;
    socklen_t length = sizeof(address);
    if (!getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length))
      port = ntohs(address.sin_port);
  }
  ~Listener() { if (fd >= 0) close(fd); }
};
}  // namespace

TEST(LinuxAsyncClient, StalledTlsReturnsToApplicationAndCanBeCancelled) {
  Listener server;
  ASSERT_NE(server.port, 0);
  Supla::LinuxClient client;
  client.setSSLEnabled(true);
  client.setTimeoutMs(200);
  const auto started = std::chrono::steady_clock::now();
  int applicationTicks = 0;
  Result result = Result::Pending;
  while (result == Result::Pending) {
    const auto before = std::chrono::steady_clock::now();
    result = client.connectStep("127.0.0.1", server.port);
    const auto cost = std::chrono::steady_clock::now() - before;
    EXPECT_LT(cost, std::chrono::milliseconds(100));
    ++applicationTicks;
    delay(1);
  }
  EXPECT_EQ(result, Result::Failed);
  EXPECT_GT(applicationTicks, 20);
  EXPECT_LT(std::chrono::steady_clock::now() - started,
            std::chrono::seconds(2));
  EXPECT_FALSE(client.connected());
  EXPECT_FALSE(client.isSecure());
  EXPECT_EQ(client.connectStep("127.0.0.1", server.port), Result::Pending);
  client.stop();
  EXPECT_FALSE(client.connected());
}

TEST(LinuxAsyncClient, PlaintextAndDnsReconnectDeliverOrderedBytes) {
  Listener server;
  ASSERT_NE(server.port, 0);
  Supla::LinuxClient client;
  client.setTimeoutMs(1000);
  Result result = Result::Pending;
  for (int i = 0; i < 1000 && result == Result::Pending; ++i) {
    result = client.connectStep("localhost", server.port);
    delay(1);
  }
  ASSERT_EQ(result, Result::Connected);
  EXPECT_FALSE(client.isSecure());
  int peer = accept(server.fd, nullptr, nullptr);
  ASSERT_GE(peer, 0);
  EXPECT_EQ(client.write("first", 5), 5U);
  EXPECT_EQ(client.write("second", 6), 6U);
  for (int i = 0; i < 20 && client.hasPendingWrites(); ++i) client.service();
  char bytes[11] = {};
  timeval timeout = {1, 0};
  setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ASSERT_EQ(recv(peer, bytes, sizeof(bytes), MSG_WAITALL), 11);
  EXPECT_EQ(std::string(bytes, sizeof(bytes)), "firstsecond");
  close(peer);
  client.stop();
  EXPECT_FALSE(client.hasPendingWrites());
}

TEST(LinuxAsyncClient, CancelledDnsCannotCompleteDifferentClientOrAttempt) {
  for (int i = 0; i < 24; ++i) {
    Supla::LinuxClient client;
    client.setTimeoutMs(500);
    EXPECT_EQ(client.connectStep("localhost", 1), Result::Pending);
    client.connectStep("localhost", 1);  // Submit resolver job.
    client.stop();
    EXPECT_FALSE(client.connected());
  }
  Listener server;
  ASSERT_NE(server.port, 0);
  Supla::LinuxClient next;
  auto result = Result::Pending;
  for (int i = 0; i < 1000 && result == Result::Pending; ++i) {
    result = next.connectStep("localhost", server.port);
    delay(1);
  }
  EXPECT_EQ(result, Result::Connected);
}

namespace {
struct Certificate {
  EVP_PKEY *key = nullptr;
  X509 *certificate = nullptr;
  std::string pem;
  Certificate() {
    auto context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!context) return;
    if (EVP_PKEY_keygen_init(context) > 0 &&
        EVP_PKEY_CTX_set_rsa_keygen_bits(context, 2048) > 0)
      EVP_PKEY_keygen(context, &key);
    EVP_PKEY_CTX_free(context);
    if (!key) return;
    certificate = X509_new();
    X509_set_version(certificate, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
    X509_gmtime_adj(X509_getm_notBefore(certificate), -60);
    X509_gmtime_adj(X509_getm_notAfter(certificate), 3600);
    X509_set_pubkey(certificate, key);
    auto subject = X509_get_subject_name(certificate);
    X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0);
    X509_set_issuer_name(certificate, subject);
    for (auto entry : {std::pair<int, const char *>(NID_basic_constraints,
                                                  "critical,CA:TRUE"),
                      {NID_subject_alt_name, "DNS:localhost"}}) {
      auto extension = X509V3_EXT_conf_nid(nullptr, nullptr, entry.first,
                                         const_cast<char *>(entry.second));
      if (extension) {
        X509_add_ext(certificate, extension, -1);
        X509_EXTENSION_free(extension);
      }
    }
    X509_sign(certificate, key, EVP_sha256());
    auto bio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(bio, certificate);
    char *bytes = nullptr;
    const auto size = BIO_get_mem_data(bio, &bytes);
    pem.assign(bytes, size);
    BIO_free(bio);
  }
  ~Certificate() { X509_free(certificate); EVP_PKEY_free(key); }
};
struct TlsServer {
  Listener listener;
  std::thread thread;
  SSL_CTX *context = nullptr;
  explicit TlsServer(Certificate &certificate) {
    context = SSL_CTX_new(TLS_server_method());
    if (!context || !listener.port) return;
    SSL_CTX_use_certificate(context, certificate.certificate);
    SSL_CTX_use_PrivateKey(context, certificate.key);
    thread = std::thread([this] {
      pollfd ready = {};
      ready.fd = listener.fd;
      ready.events = POLLIN;
      if (poll(&ready, 1, 2000) <= 0) return;
      int peer = accept(listener.fd, nullptr, nullptr);
      if (peer < 0) return;
      timeval timeout = {2, 0};
      setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      setsockopt(peer, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      SSL *ssl = SSL_new(context);
      SSL_set_fd(ssl, peer);
      if (SSL_accept(ssl) == 1) {
        char bytes[5];
        if (SSL_read(ssl, bytes, sizeof(bytes)) == 5)
          SSL_write(ssl, "reply", 5);
      }
      SSL_free(ssl);
      close(peer);
    });
  }
  ~TlsServer() {
    if (thread.joinable()) thread.join();
    SSL_CTX_free(context);
  }
};
}  // namespace

TEST(LinuxAsyncClient, ValidTlsPreservesCaAndHostnameAndTransfersData) {
  Certificate certificate;
  ASSERT_FALSE(certificate.pem.empty());
  TlsServer server(certificate);
  ASSERT_NE(server.listener.port, 0);
  Supla::LinuxClient client;
  client.setTimeoutMs(2000);
  client.setSSLEnabled(true);
  client.setCACert(certificate.pem.c_str());
  Result result = Result::Pending;
  for (int i = 0; i < 2000 && result == Result::Pending; ++i) {
    result = client.connectStep("localhost", server.listener.port);
    delay(1);
  }
  ASSERT_EQ(result, Result::Connected);
  EXPECT_TRUE(client.isSecure());
  ASSERT_EQ(client.write("hello", 5), 5U);
  char reply[5] = {};
  size_t used = 0;
  for (int i = 0; i < 2000 && used < sizeof(reply); ++i) {
    client.service();
    int count = client.read(reply + used, sizeof(reply) - used);
    if (count > 0) used += count;
    delay(1);
  }
  EXPECT_EQ(used, 5U);
  EXPECT_EQ(std::string(reply, used), "reply");
}

TEST(LinuxAsyncClient, WrongServerIdentityFailsWithoutPlaintextFallback) {
  Certificate certificate;
  ASSERT_FALSE(certificate.pem.empty());
  TlsServer server(certificate);
  ASSERT_NE(server.listener.port, 0);
  Supla::LinuxClient client;
  client.setTimeoutMs(2000);
  client.setSSLEnabled(true);
  client.setCACert(certificate.pem.c_str());
  Result result = Result::Pending;
  for (int i = 0; i < 2000 && result == Result::Pending; ++i) {
    // Certificate authorizes localhost, not a numeric IP identity.
    result = client.connectStep("127.0.0.1", server.listener.port);
    delay(1);
  }
  EXPECT_EQ(result, Result::Failed);
  EXPECT_FALSE(client.isSecure());
  EXPECT_FALSE(client.connected());
  EXPECT_FALSE(client.hasPendingWrites());
}
#endif  // SUPLA_ASYNC_LINUX_TEST
