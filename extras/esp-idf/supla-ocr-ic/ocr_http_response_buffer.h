// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_OCR_IC_OCR_HTTP_RESPONSE_BUFFER_H_
#define EXTRAS_ESP_IDF_SUPLA_OCR_IC_OCR_HTTP_RESPONSE_BUFFER_H_

#include <stddef.h>
#include <string.h>

namespace Supla {
namespace Sensor {

inline bool appendOcrHttpResponseData(char *buffer,
                                      size_t bufferSize,
                                      size_t *offset,
                                      const char *data,
                                      size_t dataSize) {
  if (buffer == nullptr || bufferSize == 0 || offset == nullptr ||
      *offset >= bufferSize || (data == nullptr && dataSize != 0)) {
    return false;
  }

  const size_t available = bufferSize - 1 - *offset;
  if (dataSize > available) {
    buffer[*offset] = '\0';
    return false;
  }

  if (dataSize != 0) {
    memcpy(buffer + *offset, data, dataSize);
    *offset += dataSize;
  }
  buffer[*offset] = '\0';
  return true;
}

}  // namespace Sensor
}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_OCR_IC_OCR_HTTP_RESPONSE_BUFFER_H_
