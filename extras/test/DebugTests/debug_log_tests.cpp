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

#include <gtest/gtest.h>

#include <supla-common/log.h>
#include <supla/debug/debug_log.h>

#include <string>

namespace {

class CapturingLogSink : public Supla::Debug::LogSink {
 public:
  void writeLog(int priority, const char *message) override {
    calls++;
    lastPriority = priority;
    lastMessage = message == nullptr ? "" : message;
  }

  int calls = 0;
  int lastPriority = -1;
  std::string lastMessage;
};

}  // namespace

TEST(DebugLogTests, WritesToRegisteredSink) {
  CapturingLogSink sink;
  Supla::Debug::setLogSink(&sink);

  Supla::Debug::writeLog(LOG_INFO, "hello");

  EXPECT_EQ(1, sink.calls);
  EXPECT_EQ(LOG_INFO, sink.lastPriority);
  EXPECT_EQ("hello", sink.lastMessage);

  Supla::Debug::clearLogSink(&sink);
}

TEST(DebugLogTests, ClearIgnoresDifferentSink) {
  CapturingLogSink sink;
  CapturingLogSink other;
  Supla::Debug::setLogSink(&sink);

  Supla::Debug::clearLogSink(&other);
  Supla::Debug::writeLog(LOG_WARNING, "still active");

  EXPECT_EQ(1, sink.calls);
  EXPECT_EQ(LOG_WARNING, sink.lastPriority);
  EXPECT_EQ("still active", sink.lastMessage);

  Supla::Debug::clearLogSink(&sink);
}

TEST(DebugLogTests, CBridgeFormatsVarargs) {
  CapturingLogSink sink;
  Supla::Debug::setLogSink(&sink);

  supla_debug_logf(LOG_DEBUG, "value=%d", 42);

  EXPECT_EQ(1, sink.calls);
  EXPECT_EQ(LOG_DEBUG, sink.lastPriority);
  EXPECT_EQ("value=42", sink.lastMessage);

  Supla::Debug::clearLogSink(&sink);
}

TEST(DebugLogTests, PriorityPrefixesMatchSuplaLogLevels) {
  EXPECT_STREQ("ERR", Supla::Debug::logPriorityPrefix(LOG_ERR));
  EXPECT_STREQ("WAR", Supla::Debug::logPriorityPrefix(LOG_WARNING));
  EXPECT_STREQ("INF", Supla::Debug::logPriorityPrefix(LOG_INFO));
  EXPECT_STREQ("DEB", Supla::Debug::logPriorityPrefix(LOG_DEBUG));
  EXPECT_STREQ("VER", Supla::Debug::logPriorityPrefix(LOG_VERBOSE));
}
