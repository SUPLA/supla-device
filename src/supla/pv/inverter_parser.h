// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_PV_INVERTER_PARSER_H_
#define SRC_SUPLA_PV_INVERTER_PARSER_H_

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

namespace Supla {
namespace PV {
namespace detail {

inline bool onlyWhitespace(const char *text) {
  while (*text != '\0') {
    if (!isspace(static_cast<unsigned char>(*text++))) {
      return false;
    }
  }
  return true;
}

template <typename Number = double, typename Measurement>
bool parseInverterValue(const char *text,
                        uint32_t scale,
                        Measurement *measurement) {
  if (text == nullptr) {
    return false;
  }
  char *end = nullptr;
  errno = 0;
  double parsed = strtod(text, &end);
  double maximum = sizeof(Number) == sizeof(float) ? FLT_MAX : DBL_MAX;
  if (end == text || errno == ERANGE || !onlyWhitespace(end) ||
      !isfinite(parsed) || parsed < -maximum || parsed > maximum) {
    return false;
  }
  // Preserve the integration's arithmetic precision and scale, but check the
  // destination's range before converting to an integer. An exclusive upper
  // bound avoids rounding UINT64_MAX up to 2^64 when represented as double.
  double scaled = static_cast<Number>(parsed) * static_cast<Number>(scale);
  bool isSigned = static_cast<Measurement>(-1) < static_cast<Measurement>(1);
  int digits = sizeof(Measurement) * CHAR_BIT - (isSigned ? 1 : 0);
  double upper = ldexp(1.0, digits);
  double lower = isSigned ? -upper : 0;
  if (!isfinite(scaled) || scaled < lower || scaled >= upper) {
    return false;
  }
  *measurement = static_cast<Measurement>(scaled);
  return true;
}

}  // namespace detail
}  // namespace PV
}  // namespace Supla

#endif  // SRC_SUPLA_PV_INVERTER_PARSER_H_
