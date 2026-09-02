// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla/channel.h>
#include <supla/sensor/ocr_impulse_counter.h>

#include <array>
#include <string>

#include "../../esp-idf/supla-ocr-ic/ocr_http_response_buffer.h"

namespace {

class TestOcrImpulseCounter : public Supla::Sensor::OcrImpulseCounter {
 public:
  void parse(const std::string &status) {
    parseStatus(status.c_str(), status.size() + 1);
  }

 protected:
  bool takePhoto() override { return false; }
  void releasePhoto() override {}
  void setLedState(int) override {}
  bool sendPhotoToOcrServer(const char *,
                            const char *,
                            char *,
                            int,
                            const char *) override {
    return false;
  }
  bool getStatusFromOcrServer(
      const char *, const char *, char *, int) override {
    return false;
  }
};

std::string makeStatus(const std::string &measurement) {
  return "{\"id\":\"86e2e33b-0dda-443f-8c7a-e8cccfd7cf7d\","
         "\"processedAt\":\"2024-08-01T11:34:53+00:00\","
         "\"measurementValid\":true,\"resultMeasurement\":" +
         measurement + "}";
}

class OcrImpulseCounterTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(OcrImpulseCounterTests, Accepts99ByteResultMeasurement) {
  TestOcrImpulseCounter counter;

  counter.parse(makeStatus(std::string(98, '0') + "7"));

  EXPECT_EQ(counter.getCounter(), 7);
}

TEST_F(OcrImpulseCounterTests, Rejects100ByteResultMeasurement) {
  TestOcrImpulseCounter counter;

  counter.parse(makeStatus(std::string(99, '0') + "7"));

  EXPECT_EQ(counter.getCounter(), 0);
}

TEST(OcrHttpResponseBufferTests, RejectsPreviousFullBufferLimit) {
  std::array<char, 10> storage = {};
  storage.front() = 'L';
  storage.back() = 'R';
  size_t offset = 0;

  EXPECT_FALSE(Supla::Sensor::appendOcrHttpResponseData(
      storage.data() + 1, 8, &offset, "12345678", 8));
  EXPECT_EQ(offset, 0);
  EXPECT_EQ(storage[1], '\0');
  EXPECT_EQ(storage.front(), 'L');
  EXPECT_EQ(storage.back(), 'R');
}

TEST(OcrHttpResponseBufferTests, HandlesFragmentsAtBufferCapacity) {
  std::array<char, 8> buffer = {};
  size_t offset = 0;

  EXPECT_TRUE(Supla::Sensor::appendOcrHttpResponseData(
      buffer.data(), buffer.size(), &offset, "1234", 4));
  EXPECT_TRUE(Supla::Sensor::appendOcrHttpResponseData(
      buffer.data(), buffer.size(), &offset, "567", 3));
  EXPECT_STREQ(buffer.data(), "1234567");
  EXPECT_EQ(offset, 7);

  EXPECT_FALSE(Supla::Sensor::appendOcrHttpResponseData(
      buffer.data(), buffer.size(), &offset, "8", 1));
  EXPECT_STREQ(buffer.data(), "1234567");
  EXPECT_EQ(offset, 7);
}

}  // namespace
