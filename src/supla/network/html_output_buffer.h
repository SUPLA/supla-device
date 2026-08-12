// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_OUTPUT_BUFFER_H_
#define SRC_SUPLA_NETWORK_HTML_OUTPUT_BUFFER_H_

#include <stddef.h>

namespace Supla {

constexpr int SUPLA_HTML_OUTPUT_BUFFER_SIZE = 512;

class HtmlOutputBuffer {
 public:
  using FlushCallback = bool (*)(void *context, const char *buf, int size);

  explicit HtmlOutputBuffer(char *buffer = nullptr, int bufferLen = 0);

  void setBuffer(char *buffer, int bufferLen);
  void send(void *context,
            FlushCallback flushCallback,
            const char *buf,
            int size = -1);
  bool flush(void *context, FlushCallback flushCallback);
  bool error() const;

 private:
  bool flushPending(void *context, FlushCallback flushCallback);

  char *buffer_ = nullptr;
  int bufferLen_ = 0;
  int bufferPos_ = 0;
  bool error_ = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_OUTPUT_BUFFER_H_
