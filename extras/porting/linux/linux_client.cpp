// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>
#include <supla/log_wrapper.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509_vfy.h>

#include "linux_client.h"

Supla::LinuxClient::LinuxClient() {
}

Supla::LinuxClient::~LinuxClient() {
  stop();
  if (ctx) {
    SSL_CTX_free(ctx);
    ctx = nullptr;
  }
}

bool Supla::LinuxClient::setupSslContext() {
  if (ctx) {
    SSL_CTX_free(ctx);
    ctx = nullptr;
  }

  const SSL_METHOD *method = TLS_client_method();
  ctx = SSL_CTX_new(method);
  if (ctx == nullptr) {
    SUPLA_LOG_ERROR("SSL_CTX_new failed");
    return false;
  }

  if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1) {
    SUPLA_LOG_ERROR("Failed to set minimum TLS version");
    SSL_CTX_free(ctx);
    ctx = nullptr;
    return false;
  }

  if (rootCACert == nullptr && !useDefaultCACerts) {
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    return true;
  }

  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
  if (rootCACert == nullptr) {
    if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
      SUPLA_LOG_ERROR("Failed to load default CA certificates");
      SSL_CTX_free(ctx);
      ctx = nullptr;
      return false;
    }
    return true;
  }

  BIO *caBio = BIO_new_mem_buf(rootCACert, -1);
  if (caBio == nullptr) {
    SUPLA_LOG_ERROR("Failed to create CA BIO");
    SSL_CTX_free(ctx);
    ctx = nullptr;
    return false;
  }

  X509_STORE *store = SSL_CTX_get_cert_store(ctx);
  if (store == nullptr) {
    SUPLA_LOG_ERROR("Failed to get SSL certificate store");
    BIO_free(caBio);
    SSL_CTX_free(ctx);
    ctx = nullptr;
    return false;
  }

  int certsLoaded = 0;
  ERR_clear_error();
  while (true) {
    X509 *caCert = PEM_read_bio_X509(caBio, nullptr, 0, nullptr);
    if (caCert == nullptr) {
      break;
    }

    const int addResult = X509_STORE_add_cert(store, caCert);
    if (addResult != 1) {
      uint64_t err = ERR_peek_last_error();
      if (ERR_GET_REASON(err) != X509_R_CERT_ALREADY_IN_HASH_TABLE) {
        SUPLA_LOG_ERROR("Failed to add CA certificate to SSL context");
        X509_free(caCert);
        BIO_free(caBio);
        SSL_CTX_free(ctx);
        ctx = nullptr;
        return false;
      }
      ERR_clear_error();
    }

    certsLoaded++;
    X509_free(caCert);
  }

  uint64_t pemError = ERR_peek_last_error();
  BIO_free(caBio);

  if (certsLoaded == 0) {
    SUPLA_LOG_ERROR("Failed to read CA certificate");
    SSL_CTX_free(ctx);
    ctx = nullptr;
    return false;
  }

  if (pemError != 0 &&
      ERR_GET_REASON(pemError) != PEM_R_NO_START_LINE) {
    SUPLA_LOG_ERROR("Failed to parse CA certificate bundle");
    SSL_CTX_free(ctx);
    ctx = nullptr;
    ERR_clear_error();
    return false;
  }

  ERR_clear_error();
  return true;
}

int Supla::LinuxClient::connectImp(const char *server, uint16_t port) {
  stop();

  struct addrinfo hints = {};
  struct addrinfo *addresses = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  char portStr[10] = {};
  snprintf(portStr, sizeof(portStr), "%d", port);

  const int status = getaddrinfo(server, portStr, &hints, &addresses);
  if (status != 0) {
    SUPLA_LOG_ERROR("%s: %s", server, gai_strerror(status));
    return 0;
  }

  int err = 0;
  int flagsCopy = 0;
  for (struct addrinfo *addr = addresses; addr != nullptr;
       addr = addr->ai_next) {
    connectionFd = socket(
        addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (connectionFd == -1) {
      err = errno;
      continue;
    }

    flagsCopy = ::fcntl(connectionFd, F_GETFL, 0);
    if (flagsCopy == -1) {
      err = errno;
      ::close(connectionFd);
      connectionFd = -1;
      continue;
    }
    struct timeval timeout = {};
    timeout.tv_sec = 10;
    ::setsockopt(
        connectionFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(
        connectionFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (::fcntl(connectionFd, F_SETFL, flagsCopy | O_NONBLOCK) == -1) {
      err = errno;
      ::close(connectionFd);
      connectionFd = -1;
      continue;
    }
    if (::connect(connectionFd, addr->ai_addr, addr->ai_addrlen) == 0) {
      if (::fcntl(connectionFd, F_SETFL, flagsCopy) == -1) {
        err = errno;
        srcIp = 0;
        ::close(connectionFd);
        connectionFd = -1;
        continue;
      }
      break;
    }

    err = errno;
    bool isConnected = false;

    if (errno == EWOULDBLOCK || errno == EINPROGRESS) {
      struct pollfd pfd = {};
      pfd.fd = connectionFd;
      pfd.events = POLLOUT;

      int result = ::poll(&pfd, 1, timeoutMs);
      if (result > 0) {
        socklen_t len = sizeof(err);
        int retval =
            ::getsockopt(connectionFd, SOL_SOCKET, SO_ERROR, &err, &len);

        if (retval == 0 && err == 0) {
          isConnected = true;
        }
      }
    }

    if (::fcntl(connectionFd, F_SETFL, flagsCopy) == -1) {
      err = errno;
      srcIp = 0;
      ::close(connectionFd);
      connectionFd = -1;
      continue;
    }

    if (isConnected) {
      break;
    }
    srcIp = 0;
    ::close(connectionFd);
    connectionFd = -1;
  }

  freeaddrinfo(addresses);

  if (connectionFd == -1) {
    srcIp = 0;
    SUPLA_LOG_ERROR("%s: %s", server, strerror(err));
    return 0;
  }


  if (sslEnabled) {
    if (!setupSslContext()) {
      stop();
      return 0;
    }
    ssl = SSL_new(ctx);
    if (ssl == nullptr) {
      SUPLA_LOG_ERROR("SSL_new() failed");
      stop();
      return 0;
    }
    if (SSL_set_fd(ssl, connectionFd) != 1) {
      SUPLA_LOG_ERROR("SSL_set_fd failed");
      stop();
      return 0;
    }
    if (SSL_set_tlsext_host_name(ssl, server) != 1) {
      SUPLA_LOG_ERROR("SSL_set_tlsext_host_name failed");
      stop();
      return 0;
    }
    if (isCertificateValidationEnabled() && SSL_set1_host(ssl, server) != 1) {
      SUPLA_LOG_ERROR("SSL_set1_host failed");
      stop();
      return 0;
    }
    int ret = SSL_connect(ssl);
    if (ret <= 0) {
      printSslError(ssl, ret);
      if (isCertificateValidationEnabled()) {
        SUPLA_LOG_WARNING("SSL verify result: %s",
                          X509_verify_cert_error_string(
                              SSL_get_verify_result(ssl)));
      }
      stop();
      return 0;
    }

    SUPLA_LOG_DEBUG("TLS version: %s", SSL_get_version(ssl));
    SUPLA_LOG_DEBUG("Cipher suite: %s", SSL_get_cipher(ssl));
    if (!checkSslCerts(ssl)) {
      stop();
      return 0;
    }
  } else {
    // TODO(klew): implement non ssl connection handling for Linux
  }

  if (::fcntl(connectionFd, F_SETFL, flagsCopy | O_NONBLOCK) == -1) {
    SUPLA_LOG_ERROR("Failed to set socket nonblocking mode");
    stop();
    return 0;
  }

  // store connection source IP address
  struct sockaddr_in addr = {};
  socklen_t addrLen = sizeof(addr);
  if (getsockname(connectionFd, (struct sockaddr *)&addr, &addrLen) == 0) {
    srcIp = addr.sin_addr.s_addr;
  } else {
    srcIp = 0;
  }
  uint8_t ipArr[4];
  for (int i = 0; i < 4; i++) {
    ipArr[i] = (srcIp >> (i * 8)) & 0xFF;
  }

  SUPLA_LOG_DEBUG("Connected via IP %d.%d.%d.%d", ipArr[0], ipArr[1],
      ipArr[2], ipArr[3]);

  return 1;
}

size_t Supla::LinuxClient::writeImp(const uint8_t *buf, size_t size) {
  if (connectionFd == -1) {
    return 0;
  }

  if (sslEnabled) {
    if (ssl == nullptr) {
      return 0;
    }

    size_t sent = 0;
    while (sent < size) {
      int result = SSL_write(ssl, buf + sent, size - sent);
      if (result > 0) {
        sent += result;
        continue;
      }

      int sslError = SSL_get_error(ssl, result);
      if (sslError == SSL_ERROR_WANT_READ ||
          sslError == SSL_ERROR_WANT_WRITE) {
        struct pollfd pfd = {};
        pfd.fd = connectionFd;
        pfd.events = sslError == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT;
        int pollResult = ::poll(&pfd, 1, timeoutMs);
        if (pollResult > 0) {
          if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            stop();
            return 0;
          }
          if (pfd.revents & pfd.events) {
            continue;
          }
          stop();
          return 0;
        }
        if (pollResult == 0) {
          stop();
          return 0;
        }
        if (errno == EINTR) {
          continue;
        }
        stop();
        return 0;
      }

      printSslError(ssl, result);
      stop();
      return 0;
    }
    return sent;
  }

  size_t sent = 0;
  while (sent < size) {
    ssize_t result = ::write(connectionFd, buf + sent, size - sent);
    if (result > 0) {
      sent += result;
      continue;
    }

    if (result == 0) {
      stop();
      return 0;
    }

    if (errno == EINTR) {
      continue;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      struct pollfd pfd = {};
      pfd.fd = connectionFd;
      pfd.events = POLLOUT;
      int pollResult = ::poll(&pfd, 1, timeoutMs);
      if (pollResult > 0) {
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
          stop();
          return 0;
        }
        if (pfd.revents & POLLOUT) {
          continue;
        }
        stop();
        return 0;
      }
      if (pollResult < 0 && errno == EINTR) {
        continue;
      }
      stop();
      return 0;
    }

    stop();
    return 0;
  }
  return sent;
}

int Supla::LinuxClient::available() {
  if (connectionFd < 0) {
    return 0;
  }

  if (sslEnabled && ssl != nullptr) {
    int pending = SSL_pending(ssl);
    if (pending > 0) {
      return pending;
    }

    struct pollfd pfd = {};
    pfd.fd = connectionFd;
    pfd.events = POLLIN;
    int pollResult = ::poll(&pfd, 1, 0);
    if (pollResult <= 0) {
      return 0;
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      stop();
      return 0;
    }
    return (pfd.revents & POLLIN) ? 1 : 0;
  }

  int value;
  int error = ioctl(connectionFd, FIONREAD, &value);

  if (error) {
    stop();
    return -1;
  }

  return value;
}

int Supla::LinuxClient::readImp(uint8_t *buf, size_t size) {
  ssize_t response = 0;
  if (buf == nullptr || size == 0) {
    return -1;
  }

  if (connectionFd < 0) {
    return 0;
  }

  if (sslEnabled) {
    if (ssl == nullptr) {
      return 0;
    }
    response = SSL_read(ssl, buf, size);
    if (response > 0) {
      return response;
    } else {
      int sslError = SSL_get_error(ssl, response);
      bool connectionClosed = false;

      switch (sslError) {
        case SSL_ERROR_WANT_READ: {
          break;
        }
        case SSL_ERROR_WANT_WRITE: {
          break;
        }
        case SSL_ERROR_ZERO_RETURN: {
          SUPLA_LOG_INFO("Connection closed by peer");
          stop();
          connectionClosed = true;
          break;
        }
        case SSL_ERROR_SYSCALL: {
          if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
          }
          SUPLA_LOG_WARNING(
              "Client: SSL_ERROR_SYSCALL non-recoverable, fatal I/O error"
              " occurred (errno: %d)", errno);
          stop();
          connectionClosed = true;
          break;
        }
        case SSL_ERROR_SSL: {
          SUPLA_LOG_WARNING(
              "Client: SSL_ERROR_SSL non-recoverable, fatal error in the SSL "
              "library occurred");
          printSslError(ssl, response);
          stop();
          connectionClosed = true;
          break;
        }
        default: {
          printSslError(ssl, response);
          stop();
          connectionClosed = true;
        }
      }
      if (connectionClosed) {
        return 0;
      }
    }

    return -1;

  } else {
    response = ::read(connectionFd, buf, size);

    if (response == 0) {
      SUPLA_LOG_DEBUG("read response == 0");
      stop();
      return 0;
    }

    if (response < 0) {
      SUPLA_LOG_DEBUG("read response == %d", response);
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
      }
      stop();
      return 0;
    }
  }

  return response;
}

void Supla::LinuxClient::stop() {
  if (ssl) {
    SSL_free(ssl);
  }
  if (connectionFd >= 0) {
    close(connectionFd);
  }
  connectionFd = -1;
  ssl = nullptr;
  srcIp = 0;
}

uint8_t Supla::LinuxClient::connected() {
  if (connectionFd == -1) {
    return false;
  }

  char tmp;
  ssize_t response = ::recv(connectionFd, &tmp, 1, MSG_DONTWAIT | MSG_PEEK);
  if (response == 0) {
    return false;
  }

  if (response == -1) {
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      return true;
    }
    return false;
  }
  return true;
}

void Supla::LinuxClient::setTimeoutMs(uint16_t _timeoutMs) {
  timeoutMs = _timeoutMs;
}

void Supla::LinuxClient::setUseDefaultCACerts(bool useDefault) {
  useDefaultCACerts = useDefault;
}

bool Supla::LinuxClient::isCertificateValidationEnabled() const {
  return rootCACert != nullptr || useDefaultCACerts;
}

bool Supla::LinuxClient::checkSslCerts(SSL *ssl) {
  X509 *cert = nullptr;
  char *line;
  const int64_t verifyResult = SSL_get_verify_result(ssl);

  cert = SSL_get_peer_certificate(ssl);
  if (cert != NULL) {
    if (isCertificateValidationEnabled() && verifyResult != X509_V_OK) {
      SUPLA_LOG_WARNING("Failed to verify server certificate: %s",
                        X509_verify_cert_error_string(verifyResult));
      X509_free(cert);
      return false;
    }
    SUPLA_LOG_DEBUG("Server certificates:");
    line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
    SUPLA_LOG_DEBUG("Subject: %s", line ? line : "(null)");
    if (line) {
      free(line);
    }
    line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
    SUPLA_LOG_DEBUG("Issuer: %s", line ? line : "(null)");
    if (line) {
      free(line);
    }
    X509_free(cert);
    return true;
  } else {
    SUPLA_LOG_WARNING("Failed to verify server certificate");
    return false;
  }
}

int32_t Supla::LinuxClient::printSslError(SSL *ssl, int ret_code) {
  if (ssl == nullptr) {
    SUPLA_LOG_ERROR("SSL object is null");
    return -1;
  }

  int32_t ssl_error;

  ssl_error = SSL_get_error(ssl, ret_code);

  switch (ssl_error) {
    case SSL_ERROR_NONE:
      SUPLA_LOG_ERROR("SSL_ERROR_NONE");
      break;
    case SSL_ERROR_SSL:
      SUPLA_LOG_ERROR("SSL_ERROR_SSL");
      break;
    case SSL_ERROR_WANT_READ:
      SUPLA_LOG_ERROR("SSL_ERROR_WANT_READ");
      break;
    case SSL_ERROR_WANT_WRITE:
      SUPLA_LOG_ERROR("SSL_ERROR_WANT_WRITE");
      break;
    case SSL_ERROR_WANT_X509_LOOKUP:
      SUPLA_LOG_ERROR("SSL_ERROR_WANT_X509_LOOKUP");
      break;
    case SSL_ERROR_SYSCALL:
      SUPLA_LOG_ERROR("SSL_ERROR_SYSCALL");
      break;
    case SSL_ERROR_ZERO_RETURN:
      SUPLA_LOG_ERROR("SSL_ERROR_ZERO_RETURN");
      break;
    case SSL_ERROR_WANT_CONNECT:
      SUPLA_LOG_ERROR("SSL_ERROR_WANT_CONNECT");
      break;
    case SSL_ERROR_WANT_ACCEPT:
      SUPLA_LOG_ERROR("SSL_ERROR_WANT_ACCEPT");
      break;
  }

  uint64_t err = 0;
  while ((err = ERR_get_error()) != 0) {
    char errBuf[256] = {};
    ERR_error_string_n(err, errBuf, sizeof(errBuf));
    SUPLA_LOG_ERROR("OpenSSL: %s", errBuf);
  }

  return ssl_error;
}

Supla::Client *Supla::ClientBuilder() {
  return new Supla::LinuxClient;
}
