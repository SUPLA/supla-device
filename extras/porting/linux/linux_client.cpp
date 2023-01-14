/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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


#include "linux_client.h"

Supla::LinuxClient::LinuxClient() {
}

Supla::LinuxClient::~LinuxClient() {
  if (ctx) {
    SSL_CTX_free(ctx);
    ctx = nullptr;
  }
}

int Supla::LinuxClient::connectImp(const char *server, uint16_t port) {
  struct addrinfo hints = {};
  struct addrinfo *addresses = {};
  hints.ai_family = AF_UNSPEC;
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
        addresses->ai_family, addresses->ai_socktype, addresses->ai_protocol);
    if (connectionFd == -1) {
      err = errno;
      continue;
    }

    flagsCopy = fcntl(connectionFd, F_GETFL, 0);
    fcntl(connectionFd, F_SETFL, O_NONBLOCK);
    if (::connect(connectionFd, addr->ai_addr, addr->ai_addrlen) == 0) {
      break;
    }

    err = errno;
    bool isConnected = false;

    if (errno == EWOULDBLOCK || errno == EINPROGRESS) {
      struct pollfd pfd = {};
      pfd.fd = connectionFd;
      pfd.events = POLLOUT;

      int result = poll(&pfd, 1, timeoutMs);
      if (result > 0) {
        socklen_t len = sizeof(err);
        int retval = getsockopt(connectionFd, SOL_SOCKET, SO_ERROR, &err, &len);

        if (retval == 0 && err == 0) {
          isConnected = true;
        }
      }
    }

    if (isConnected) {
      break;
    }
    close(connectionFd);
    connectionFd = -1;
  }

  freeaddrinfo(addresses);

  if (connectionFd == -1) {
    SUPLA_LOG_ERROR("%s: %s", server, strerror(err));
    return 0;
  }

  fcntl(connectionFd, F_SETFL, flagsCopy);

  if (sslEnabled) {
    if (ctx == nullptr) {
      const SSL_METHOD *method = TLS_client_method();
      ctx = SSL_CTX_new(method);

      if (ctx == nullptr) {
        SUPLA_LOG_ERROR("SSL_CTX_new failed");
        stop();
        return 0;
      }
      if (rootCACert) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        // TODO(klew): add custom root CA verification
      }
    }
    ssl = SSL_new(ctx);
    if (ssl == nullptr) {
      SUPLA_LOG_ERROR("SSL_new() failed");
      stop();
      return 0;
    }
    SSL_set_fd(ssl, connectionFd);
    SSL_connect(ssl);

    SUPLA_LOG_DEBUG("Connected with %s encryption", SSL_get_cipher(ssl));
    SSL_get_cipher(ssl);
    if (!checkSslCerts(ssl)) {
      stop();
      return 0;
    }
  } else {
    // TODO(klew): implement non ssl connection handling for Linux
  }

  fcntl(connectionFd, F_SETFL, O_NONBLOCK);

  return 1;
}

size_t Supla::LinuxClient::writeImp(const uint8_t *buf, size_t size) {
  if (connectionFd == -1) {
    return 0;
  }

  int result = 0;
  if (sslEnabled) {
    result = SSL_write(ssl, buf, size);
    if (result <= 0) {
      printSslError(ssl, result);
      stop();
    }

  } else {
    int result = ::write(connectionFd, buf, size);
    if (result < 0) {
      stop();
    }
  }
  return result;
}

int Supla::LinuxClient::available() {
  if (connectionFd < 0) {
    return 0;
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
  if (buf == nullptr || size <= 0) {
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

      switch (sslError) {
        case SSL_ERROR_WANT_READ: {
          break;
        }
        case SSL_ERROR_ZERO_RETURN: {
          SUPLA_LOG_INFO("Connection closed by peer");
          stop();
          break;
        }
        case SSL_ERROR_SYSCALL: {
          SUPLA_LOG_WARNING(
              "Client: SSL_ERROR_SYSCALL non-recoverable, fatal I/O error"
              " occurred (errno: %d)", errno);
          stop();
          break;
        }
        case SSL_ERROR_SSL: {
          SUPLA_LOG_WARNING(
              "Client: SSL_ERROR_SSL non-recoverable, fatal error in the SSL "
              "library occurred");
          stop();
          break;
        }
        default: {
          printSslError(ssl, response);
        }
      }
    }

    return -1;

  } else {
    response = ::read(connectionFd, buf, size);

    if (response == 0) {
      SUPLA_LOG_DEBUG("read response == 0");
      stop();
      return -1;
    }

    if (response < 0) {
      SUPLA_LOG_DEBUG("read response == %d", response);
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
      }
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

bool Supla::LinuxClient::checkSslCerts(SSL *ssl) {
  X509 *cert = nullptr;
  char *line;

  cert = SSL_get_peer_certificate(ssl);
  if (cert != NULL) {
    SUPLA_LOG_DEBUG("Server certificates:");
    line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
    SUPLA_LOG_DEBUG("Subject: %s", line);
    free(line);
    line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
    SUPLA_LOG_DEBUG("Issuer: %s", line);
    free(line);
    X509_free(cert);
    return true;
  } else {
    SUPLA_LOG_WARNING("Failed to verify server certificate");
    return false;
  }
}

int32_t Supla::LinuxClient::printSslError(SSL *ssl, int ret_code) {
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

  return ssl_error;
}

Supla::Client *Supla::ClientBuilder() {
  return new Supla::LinuxClient;
}

