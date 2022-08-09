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

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <string.h>
#include <supla/log_wrapper.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdio>

#include "network_client.h"

Supla::NetworkClient::NetworkClient() {
}

Supla::NetworkClient::~NetworkClient() {
}

int Supla::NetworkClient::connect(const char *server, uint16_t port) {
  SUPLA_LOG_DEBUG("Establishing connection with: %s (port: %d)", server, port);

  struct addrinfo hints = {0};
  struct addrinfo *addresses;
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

      int result = poll(&pfd, 1, 3000);
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

  fcntl(connectionFd, F_SETFL, O_NONBLOCK);

  return 1;
}

int Supla::NetworkClient::connect(IPAddress ip, uint16_t port) {
  char server[100] = {};
  snprintf(server,
           99,
           "%d.%d.%d.%d",
           ip.addr[0],
           ip.addr[1],
           ip.addr[2],
           ip.addr[3]);
  return connect(server, port);
}

std::size_t Supla::NetworkClient::write(uint8_t data) {
  if (connectionFd == -1) {
    return 0;
  }

  return ::write(connectionFd, &data, sizeof(data));
}

std::size_t Supla::NetworkClient::write(const uint8_t *buf, std::size_t size) {
  if (connectionFd == -1) {
    return 0;
  }

  int response = ::write(connectionFd, buf, size);
  if (response < 0) {
    stop();
  }
  return response;
}

int Supla::NetworkClient::available() {
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

int Supla::NetworkClient::read() {
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

int Supla::NetworkClient::read(uint8_t *buf, std::size_t size) {
  if (buf == nullptr || size <= 0) {
    return -1;
  }

  ssize_t response = ::read(connectionFd, buf, size);

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

  return response;
}

int Supla::NetworkClient::read(char *buf, std::size_t size) {
  return read(reinterpret_cast<uint8_t *>(buf), size);
}

void Supla::NetworkClient::stop() {
  if (connectionFd >= 0) {
    close(connectionFd);
    connectionFd = -1;
  }
}

uint8_t Supla::NetworkClient::connected() {
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

std::size_t Supla::NetworkClient::println() {
  return println("");
}

std::size_t Supla::NetworkClient::println(const char *str) {
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

std::size_t Supla::NetworkClient::print(const char *str) {
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
