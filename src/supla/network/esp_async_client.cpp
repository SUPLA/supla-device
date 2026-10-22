// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_async_client.h"
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <lwip/sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>
#include <mbedtls/x509_crt.h>
#if MBEDTLS_VERSION_MAJOR >= 4 || defined(MBEDTLS_USE_PSA_CRYPTO)
#include <psa/crypto.h>
#endif
#include <supla/crypto.h>
#include <supla/log_wrapper.h>
#include <new>

struct Supla::EspAsyncClient::Tls {
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config config;
  mbedtls_x509_crt ca;
  Tls() {
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_x509_crt_init(&ca);
  }
  ~Tls() {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_x509_crt_free(&ca);
  }
};

namespace {
#if MBEDTLS_VERSION_MAJOR < 4
int randomBytes(void *, unsigned char *data, size_t size) {
  return Supla::Crypto::fillRandom(data, size)
      ? 0 : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
#endif
bool wouldBlock() {
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}
// TLS 1.3 delivers session tickets through mbedtls_ssl_read(); they are not
// connection errors (ESP-TLS ignored them as well).
bool wantsRead(int result) {
#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
  if (result == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) return true;
#endif
  return result == MBEDTLS_ERR_SSL_WANT_READ;
}
}  // namespace

Supla::EspAsyncClient::~EspAsyncClient() { stop(); }

Supla::Client::ConnectResult Supla::EspAsyncClient::startTransport(
    const char *host, uint16_t port) {
  port_ = port;
  phase_ = Phase::Dns;
  noteProgress(kDnsTimeoutMs);
  if (!dns_.start(host)) {
    connectionError_ = ConnectionError::DNS;
    setFailure(kCannotResolveHostname);
    return ConnectResult::Failed;
  }
  return ConnectResult::Pending;
}

bool Supla::EspAsyncClient::setupTls() {
  tls_ = new (std::nothrow) Tls;
  if (!tls_) {
    SUPLA_LOG_ERROR("ESP TLS INIT FAILED");
    return false;
  }
#if MBEDTLS_VERSION_MAJOR >= 4 || defined(MBEDTLS_USE_PSA_CRYPTO)
  if (psa_crypto_init() != PSA_SUCCESS) return false;
#endif
  int result = mbedtls_ssl_config_defaults(&tls_->config, MBEDTLS_SSL_IS_CLIENT,
      MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  if (result) {
    tlsFailed(kTlsSetupFailed, result);
    return false;
  }
#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_ssl_conf_rng(&tls_->config, randomBytes, nullptr);
#endif
  // Preserve the existing configured CA/insecure policy; no automatic fallback.
  if (rootCACert) {
    result = mbedtls_x509_crt_parse(&tls_->ca,
        reinterpret_cast<const unsigned char *>(rootCACert),
        strlen(rootCACert) + 1);
    if (result) {
      setFailure(kCaParseFailed, -result);
      connectionError_ = ConnectionError::CERTIFICATE_ERROR;
      return false;
    }
    mbedtls_ssl_conf_ca_chain(&tls_->config, &tls_->ca, nullptr);
    mbedtls_ssl_conf_authmode(&tls_->config, MBEDTLS_SSL_VERIFY_REQUIRED);
  } else {
#if defined(ARDUINO_ARCH_ESP32) || \
    defined(CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY)
    mbedtls_ssl_conf_authmode(&tls_->config, MBEDTLS_SSL_VERIFY_NONE);
#else
    // Match ESP-TLS builds that prohibit connections without a trust option.
    connectionError_ = ConnectionError::CERTIFICATE_ERROR;
    return false;
#endif
  }
  result = mbedtls_ssl_setup(&tls_->ssl, &tls_->config);
  if (!result) result = mbedtls_ssl_set_hostname(&tls_->ssl, connectionHost());
  if (result) {
    tlsFailed(kTlsSetupFailed, result);
    return false;
  }
  mbedtls_ssl_set_bio(&tls_->ssl, this, sendSocket, receiveSocket, nullptr);
  return true;
}

Supla::Client::ConnectResult Supla::EspAsyncClient::pollTransport() {
  if (phase_ == Phase::Dns) {
    uint32_t address = 0;
    const int result = dns_.poll(&address);
    if (!result) return ConnectResult::Pending;
    if (result < 0) {
      connectionError_ = ConnectionError::DNS;
      setFailure(kCannotResolveHostname);
      return ConnectResult::Failed;
    }
    phase_ = Phase::Tcp;
    noteProgress();
    fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd_ < 0 || fcntl(fd_, F_SETFL, O_NONBLOCK) < 0) {
      setFailure(kCannotCreateSocket);
      return ConnectResult::Failed;
    }
    sockaddr_in remote = {};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port_);
    remote.sin_addr.s_addr = address;
    const int status = ::connect(fd_, reinterpret_cast<sockaddr *>(&remote),
                                 sizeof(remote));
    if (status < 0 && errno != EINPROGRESS && !wouldBlock()) {
      connectionError_ = ConnectionError::CONNECTION_REFUSED;
      setFailure(kFailedConnectToHost);
      return ConnectResult::Failed;
    }
    return ConnectResult::Pending;
  }
  if (phase_ == Phase::Tcp) {
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(fd_, &writable);
    timeval timeout = {};
    const int ready = select(fd_ + 1, nullptr, &writable, nullptr, &timeout);
    if (ready < 0 && !wouldBlock()) {
      setFailure(kFailedConnectToHost);
      return ConnectResult::Failed;
    }
    if (ready <= 0) return ConnectResult::Pending;
    int error = 0;
    socklen_t length = sizeof(error);
    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &length) || error) {
      connectionError_ = ConnectionError::CONNECTION_REFUSED;
      setFailure(kFailedConnectToHost);
      return ConnectResult::Failed;
    }
    sockaddr_in local = {};
    length = sizeof(local);
    if (!getsockname(fd_, reinterpret_cast<sockaddr *>(&local), &length))
      srcIp = local.sin_addr.s_addr;
    if (!sslEnabled) return ConnectResult::Connected;
    phase_ = Phase::Tls;
    noteProgress();
    if (setupTls()) return ConnectResult::Pending;
    setFailure(kTlsSetupFailed);
    return ConnectResult::Failed;
  }
  const int result = mbedtls_ssl_handshake(&tls_->ssl);
  if (!result) return ConnectResult::Connected;
  if (result == MBEDTLS_ERR_SSL_WANT_READ ||
      result == MBEDTLS_ERR_SSL_WANT_WRITE)
    return ConnectResult::Pending;
  tlsFailed(kTlsHandshakeFailed, result);
  return ConnectResult::Failed;
}

void Supla::EspAsyncClient::tlsFailed(Failure failure, int result) {
  setFailure(failure, -result);
  connectionError_ = ConnectionError::TLS_ERROR;
  const uint32_t flags = tls_ ? mbedtls_ssl_get_verify_result(&tls_->ssl) : 0;
  if (flags == static_cast<uint32_t>(-1)) {
    // No verification result yet: the handshake failed before it.
  } else if (flags & MBEDTLS_X509_BADCERT_EXPIRED) {
    connectionError_ = ConnectionError::CERTIFICATE_EXPIRED;
  } else if (flags & MBEDTLS_X509_BADCERT_FUTURE) {
    connectionError_ = ConnectionError::CERTIFICATE_NOT_YET_VALID;
  } else if (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
    connectionError_ = ConnectionError::HOSTNAME_MISMATCH;
  } else if (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
    connectionError_ = ConnectionError::UNTRUSTED_CERTIFICATE;
  } else if (flags || result == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
    connectionError_ = ConnectionError::CERTIFICATE_ERROR;
  }
  SUPLA_LOG_WARNING("TLS connection failed error=%d verify_flags=%u",
                    result, static_cast<unsigned>(flags));
}

int Supla::EspAsyncClient::sendSocket(void *context, const unsigned char *data,
                                     size_t size) {
  auto client = static_cast<EspAsyncClient *>(context);
  const int result = ::send(client->fd_, data, size, MSG_DONTWAIT);
  if (result >= 0) {
    if (result > 0 && !client->connectionReady()) client->noteProgress();
    return result;
  }
  return wouldBlock() ? MBEDTLS_ERR_SSL_WANT_WRITE
                      : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

int Supla::EspAsyncClient::receiveSocket(void *context, unsigned char *data,
                                        size_t size) {
  auto client = static_cast<EspAsyncClient *>(context);
  const int result = ::recv(client->fd_, data, size, MSG_DONTWAIT);
  if (result >= 0) {
    if (result > 0 && !client->connectionReady()) client->noteProgress();
    return result;
  }
  return wouldBlock() ? MBEDTLS_ERR_SSL_WANT_READ
                      : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

void Supla::EspAsyncClient::closeTransport() {
  dns_.cancel();
  delete tls_;
  tls_ = nullptr;
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
}

bool Supla::EspAsyncClient::transportConnected() {
  if (tls_ && mbedtls_ssl_get_bytes_avail(&tls_->ssl)) return true;
  if (fd_ < 0) return false;
  char byte;
  const int result = ::recv(fd_, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
  return result > 0 || (result < 0 && wouldBlock());
}

bool Supla::EspAsyncClient::isSecure() const {
  return connectionReady() && sslEnabled && tls_;
}

int Supla::EspAsyncClient::available() {
  if (!connectionReady() || fd_ < 0) return 0;
  if (tls_ && mbedtls_ssl_get_bytes_avail(&tls_->ssl))
    return mbedtls_ssl_get_bytes_avail(&tls_->ssl);
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET(fd_, &readable);
  timeval timeout = {};
  return select(fd_ + 1, &readable, nullptr, nullptr, &timeout) > 0 ? 1 : 0;
}

int Supla::EspAsyncClient::transmit(const uint8_t *data, size_t size) {
  const int result = tls_ ? mbedtls_ssl_write(&tls_->ssl, data, size)
                         : sendSocket(this, data, size);
  if (wantsRead(result) || result == MBEDTLS_ERR_SSL_WANT_WRITE) return -1;
  return result > 0 ? result : 0;
}

int Supla::EspAsyncClient::readImp(uint8_t *data, size_t size) {
  if (!connectionReady() || !size) return -1;
  const int result = tls_ ? mbedtls_ssl_read(&tls_->ssl, data, size)
                         : receiveSocket(this, data, size);
  if (wantsRead(result) || result == MBEDTLS_ERR_SSL_WANT_WRITE) return -1;
  if (result <= 0) {
    connectionError_ = ConnectionError::CONNECTION_LOST;
    stop();
    return 0;
  }
  return result;
}
#endif
