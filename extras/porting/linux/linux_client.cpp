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
#include <pthread.h>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509_vfy.h>

#include "linux_client.h"

namespace {
// Bounded process-wide resolver jobs. A cancelled request owns its slot until
// getaddrinfo returns; no callback ever retains a Client pointer. DNS cannot
// force stop()/destruction to join a blocked system resolver.
struct DnsJob {
  enum State { Free, Running, Done, Abandoned } state = Free;
  char host[256] = {};
  uint32_t addresses[8] = {};
  uint8_t count = 0;
};
DnsJob dnsJobs[4];
pthread_mutex_t dnsMutex = PTHREAD_MUTEX_INITIALIZER;
void *resolveHost(void *context) {
  auto job = static_cast<DnsJob *>(context);
  addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *result = nullptr;
  uint32_t addresses[8] = {};
  uint8_t count = 0;
  if (!getaddrinfo(job->host, nullptr, &hints, &result)) {
    for (auto item = result; item && count < 8; item = item->ai_next) {
      if (item->ai_family == AF_INET)
        addresses[count++] = reinterpret_cast<sockaddr_in *>(
            item->ai_addr)->sin_addr.s_addr;
    }
  }
  if (result) freeaddrinfo(result);
  pthread_mutex_lock(&dnsMutex);
  if (job->state == DnsJob::Abandoned) {
    job->state = DnsJob::Free;
  } else {
    memcpy(job->addresses, addresses, sizeof(addresses));
    job->count = count;
    job->state = DnsJob::Done;
  }
  pthread_mutex_unlock(&dnsMutex);
  return nullptr;
}
}  // namespace

Supla::LinuxClient::LinuxClient() {
  timeoutMs = 3000;
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

Supla::Client::ConnectResult Supla::LinuxClient::startTransport(
    const char *host, uint16_t port) {
  remotePort = port;
  addressCount = addressIndex = 0;
  if (inet_pton(AF_INET, host, &addresses[0]) == 1) {
    addressCount = 1;
    return startTcp();
  }
  phase = Phase::Dns;
  return ConnectResult::Pending;
}

Supla::Client::ConnectResult Supla::LinuxClient::startTcp() {
  while (addressIndex < addressCount) {
    connectionFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectionFd < 0) return ConnectResult::Failed;
    int flags = fcntl(connectionFd, F_GETFL, 0);
    if (flags < 0 || fcntl(connectionFd, F_SETFL, flags | O_NONBLOCK) < 0)
      return ConnectResult::Failed;
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(remotePort);
    address.sin_addr.s_addr = addresses[addressIndex++];
    phase = Phase::Tcp;
    if (::connect(connectionFd, reinterpret_cast<sockaddr *>(&address),
                  sizeof(address)) == 0) return startTls();
    if (errno == EINPROGRESS || errno == EWOULDBLOCK)
      return ConnectResult::Pending;
    ::close(connectionFd);
    connectionFd = -1;
  }
  return ConnectResult::Failed;
}

Supla::Client::ConnectResult Supla::LinuxClient::startTls() {
  sockaddr_in local = {};
  socklen_t length = sizeof(local);
  if (!getsockname(connectionFd, reinterpret_cast<sockaddr *>(&local), &length))
    srcIp = local.sin_addr.s_addr;
  if (!sslEnabled) return ConnectResult::Connected;
  if (!setupSslContext()) return ConnectResult::Failed;
  ssl = SSL_new(ctx);
  if (!ssl || SSL_set_fd(ssl, connectionFd) != 1 ||
      SSL_set_tlsext_host_name(ssl, connectionHost()) != 1)
    return ConnectResult::Failed;
  if (isCertificateValidationEnabled()) {
    in_addr ip = {};
    if (inet_pton(AF_INET, connectionHost(), &ip) == 1) {
      if (!X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(ssl), connectionHost()))
        return ConnectResult::Failed;
    } else if (SSL_set1_host(ssl, connectionHost()) != 1) {
      return ConnectResult::Failed;
    }
  }
  SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
  phase = Phase::Tls;
  return ConnectResult::Pending;
}

Supla::Client::ConnectResult Supla::LinuxClient::pollTransport() {
  if (phase == Phase::Dns) {
    pthread_mutex_lock(&dnsMutex);
    if (dnsSlot < 0) {
      for (int i = 0; i < 4; ++i) {
        if (dnsJobs[i].state != DnsJob::Free) continue;
        dnsSlot = i;
        auto &job = dnsJobs[i];
        snprintf(job.host, sizeof(job.host), "%s", connectionHost());
        job.count = 0;
        job.state = DnsJob::Running;
        pthread_t thread;
        if (pthread_create(&thread, nullptr, resolveHost, &job)) {
          job.state = DnsJob::Free;
          dnsSlot = -1;
          pthread_mutex_unlock(&dnsMutex);
          return ConnectResult::Failed;
        }
        pthread_detach(thread);
        break;
      }
    }
    if (dnsSlot < 0 || dnsJobs[dnsSlot].state != DnsJob::Done) {
      pthread_mutex_unlock(&dnsMutex);
      return ConnectResult::Pending;
    }
    auto &job = dnsJobs[dnsSlot];
    memcpy(addresses, job.addresses, sizeof(addresses));
    addressCount = job.count;
    job.state = DnsJob::Free;
    dnsSlot = -1;
    pthread_mutex_unlock(&dnsMutex);
    return startTcp();
  }
  if (phase == Phase::Tcp) {
    pollfd descriptor = {};
    descriptor.fd = connectionFd;
    descriptor.events = POLLOUT;
    int ready = ::poll(&descriptor, 1, 0);
    if (ready < 0 && errno != EINTR) return ConnectResult::Failed;
    if (ready <= 0) return ConnectResult::Pending;
    int error = 0;
    socklen_t length = sizeof(error);
    if (getsockopt(connectionFd, SOL_SOCKET, SO_ERROR, &error, &length) ||
        error) {
      ::close(connectionFd);
      connectionFd = -1;
      return startTcp();
    }
    return startTls();
  }
  const int result = SSL_connect(ssl);
  if (result == 1) {
    return checkSslCerts(ssl) ? ConnectResult::Connected
                              : ConnectResult::Failed;
  }
  const int error = SSL_get_error(ssl, result);
  if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE)
    return ConnectResult::Pending;
  printSslError(ssl, result);
  return ConnectResult::Failed;
}

int Supla::LinuxClient::transmit(const uint8_t *buf, size_t size) {
  if (connectionFd < 0) return 0;
  if (sslEnabled) {
    const int result = SSL_write(ssl, buf, size);
    if (result > 0) return result;
    const int error = SSL_get_error(ssl, result);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE)
    return -1;
    return 0;
  }
  const int result = ::send(connectionFd, buf, size, MSG_NOSIGNAL);
  if (result > 0) return result;
  return result < 0 &&
      (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      ? -1 : 0;
}

int Supla::LinuxClient::available() {
  service();
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
  service();
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

void Supla::LinuxClient::closeTransport() {
  pthread_mutex_lock(&dnsMutex);
  if (dnsSlot >= 0) {
    auto &job = dnsJobs[dnsSlot];
    job.state = job.state == DnsJob::Done ? DnsJob::Free : DnsJob::Abandoned;
    dnsSlot = -1;
  }
  pthread_mutex_unlock(&dnsMutex);
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

bool Supla::LinuxClient::transportConnected() {
  if (ssl && SSL_pending(ssl) > 0) return true;
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

void Supla::LinuxClient::setUseDefaultCACerts(bool useDefault) {
  if (useDefaultCACerts != useDefault) stop();
  useDefaultCACerts = useDefault;
}

bool Supla::LinuxClient::isCertificateValidationEnabled() const {
  return rootCACert != nullptr || useDefaultCACerts;
}

bool Supla::LinuxClient::isSecure() const {
  return connectionReady() && ssl && connectionFd >= 0 &&
      SSL_is_init_finished(ssl);
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
