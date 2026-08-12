/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef SRC_SUPLA_NETWORK_CONNECTION_ERROR_H_
#define SRC_SUPLA_NETWORK_CONNECTION_ERROR_H_

#include <stdint.h>

namespace Supla {

enum class ConnectionError : uint8_t {
  NONE,
  UNKNOWN,
  DNS,
  CONNECTION_TIMEOUT,
  CONNECTION_REFUSED,
  CONNECTION_LOST,
  SERVER_UNAVAILABLE,
  BAD_CREDENTIALS,
  NOT_AUTHORIZED,
  PROTOCOL_ERROR,
  TLS_ERROR,
  CERTIFICATE_ERROR,
  CERTIFICATE_EXPIRED,
  CERTIFICATE_NOT_YET_VALID,
  HOSTNAME_MISMATCH,
  UNTRUSTED_CERTIFICATE,
};

}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_CONNECTION_ERROR_H_
