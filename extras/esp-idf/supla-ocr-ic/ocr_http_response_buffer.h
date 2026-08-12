/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

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
