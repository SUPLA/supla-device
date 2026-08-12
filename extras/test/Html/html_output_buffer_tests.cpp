// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <supla/network/html_output_buffer.h>

namespace {

struct FlushLog {
  std::vector<std::string> chunks;
  bool fail = false;
};

bool flushChunk(void *context, const char *buf, int size) {
  auto *log = static_cast<FlushLog *>(context);
  if (log == nullptr || buf == nullptr || size < 0 || log->fail) {
    return false;
  }
  log->chunks.emplace_back(buf, buf + size);
  return true;
}

}  // namespace

TEST(HtmlOutputBufferTests, BuffersSmallChunksUntilFlush) {
  char buffer[8] = {};
  Supla::HtmlOutputBuffer outputBuffer(buffer, sizeof(buffer));
  FlushLog log;

  outputBuffer.send(&log, flushChunk, "ab");
  outputBuffer.send(&log, flushChunk, "cd");

  EXPECT_TRUE(log.chunks.empty());
  EXPECT_TRUE(outputBuffer.flush(&log, flushChunk));
  ASSERT_EQ(log.chunks.size(), 1u);
  EXPECT_EQ(log.chunks[0], "abcd");
}

TEST(HtmlOutputBufferTests, FlushesBeforeOverflow) {
  char buffer[8] = {};
  Supla::HtmlOutputBuffer outputBuffer(buffer, sizeof(buffer));
  FlushLog log;

  outputBuffer.send(&log, flushChunk, "abcd");
  outputBuffer.send(&log, flushChunk, "efgh");
  EXPECT_TRUE(outputBuffer.flush(&log, flushChunk));

  ASSERT_EQ(log.chunks.size(), 2u);
  EXPECT_EQ(log.chunks[0], "abcd");
  EXPECT_EQ(log.chunks[1], "efgh");
}

TEST(HtmlOutputBufferTests, FlushesMultipleChunksWhenTotalExceedsBuffer) {
  char buffer[8] = {};
  Supla::HtmlOutputBuffer outputBuffer(buffer, sizeof(buffer));
  FlushLog log;

  outputBuffer.send(&log, flushChunk, "abcd");
  outputBuffer.send(&log, flushChunk, "efg");
  outputBuffer.send(&log, flushChunk, "hi");
  EXPECT_TRUE(outputBuffer.flush(&log, flushChunk));

  ASSERT_EQ(log.chunks.size(), 2u);
  EXPECT_EQ(log.chunks[0], "abcdefg");
  EXPECT_EQ(log.chunks[1], "hi");
}

TEST(HtmlOutputBufferTests, SendsLargeChunkDirectly) {
  char buffer[8] = {};
  Supla::HtmlOutputBuffer outputBuffer(buffer, sizeof(buffer));
  FlushLog log;

  outputBuffer.send(&log, flushChunk, "0123456789");
  EXPECT_TRUE(outputBuffer.flush(&log, flushChunk));

  ASSERT_EQ(log.chunks.size(), 1u);
  EXPECT_EQ(log.chunks[0], "0123456789");
}

TEST(HtmlOutputBufferTests, StopsOnFlushFailure) {
  char buffer[8] = {};
  Supla::HtmlOutputBuffer outputBuffer(buffer, sizeof(buffer));
  FlushLog log;

  outputBuffer.send(&log, flushChunk, "abcd");
  log.fail = true;
  EXPECT_FALSE(outputBuffer.flush(&log, flushChunk));
  EXPECT_TRUE(log.chunks.empty());
}
