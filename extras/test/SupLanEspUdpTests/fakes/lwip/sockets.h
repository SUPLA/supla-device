// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_LWIP_SOCKETS_H_
#define EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_LWIP_SOCKETS_H_

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
int lwip_socket(int domain, int type, int protocol);
int lwip_setsockopt(int fd, int level, int option,
                    const void *value, socklen_t length);
int lwip_bind(int fd, const sockaddr *address, socklen_t length);
int lwip_close(int fd);
int lwip_fcntl(int fd, int command, int value);
int lwip_sendto(int fd, const void *data, size_t length, int flags,
                const sockaddr *address, socklen_t addressLength);
int lwip_recvfrom(int fd, void *data, size_t capacity, int flags,
                  sockaddr *address, socklen_t *addressLength);

#endif  // EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_LWIP_SOCKETS_H_
