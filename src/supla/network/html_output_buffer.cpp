// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "html_output_buffer.h"

#include <string.h>

namespace Supla {

HtmlOutputBuffer::HtmlOutputBuffer(char *buffer, int bufferLen)
    : buffer_(buffer), bufferLen_(bufferLen) {
}

void HtmlOutputBuffer::setBuffer(char *buffer, int bufferLen) {
  buffer_ = buffer;
  bufferLen_ = bufferLen;
  bufferPos_ = 0;
  error_ = false;
}

bool HtmlOutputBuffer::flushPending(void *context,
                                    FlushCallback flushCallback) {
  if (!buffer_ || bufferPos_ <= 0) {
    return true;
  }
  if (!flushCallback) {
    return false;
  }

  if (!flushCallback(context, buffer_, bufferPos_)) {
    return false;
  }

  bufferPos_ = 0;
  buffer_[0] = '\0';
  return true;
}

bool HtmlOutputBuffer::flush(void *context, FlushCallback flushCallback) {
  if (error_) {
    return false;
  }

  if (!flushPending(context, flushCallback)) {
    error_ = true;
    return false;
  }

  return true;
}

void HtmlOutputBuffer::send(void *context,
                            FlushCallback flushCallback,
                            const char *buf,
                            int size) {
  if (error_ || buf == nullptr) {
    return;
  }

  if (size == -1) {
    size = static_cast<int>(strlen(buf));
  }
  if (size <= 0) {
    return;
  }

  if (!buffer_ || bufferLen_ <= 0) {
    if (!flushCallback || !flushCallback(context, buf, size)) {
      error_ = true;
    }
    return;
  }

  if (bufferPos_ + size + 1 > bufferLen_) {
    if (!flushPending(context, flushCallback)) {
      error_ = true;
      return;
    }
    if (size + 1 > bufferLen_) {
      if (!flushCallback || !flushCallback(context, buf, size)) {
        error_ = true;
      }
      return;
    }
  }

  memcpy(&buffer_[bufferPos_], buf, size);
  while (bufferPos_ > 0 && buffer_[bufferPos_ - 1] == '\0') {
    bufferPos_--;
  }
  bufferPos_ += size;
  buffer_[bufferPos_] = '\0';
}

bool HtmlOutputBuffer::error() const {
  return error_;
}

};  // namespace Supla
