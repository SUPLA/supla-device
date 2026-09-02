// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/device/register_device.h>
#include <supla/channels/channel.h>

#include <cstring>

namespace {

class RegisterDeviceTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::RegisterDevice::resetToDefaults();
  }

  void TearDown() override {
    Supla::RegisterDevice::resetToDefaults();
  }
};

void expectGUIDText(const char guid[SUPLA_GUID_SIZE], const char *expected) {
  char text[38];
  std::memset(text, 0x55, sizeof(text));

  Supla::RegisterDevice::setGUID(guid);
  Supla::RegisterDevice::fillGUIDText(text);

  EXPECT_STREQ(text, expected);
  EXPECT_EQ(std::strlen(text), 36U);
  EXPECT_EQ(text[36], '\0');
  EXPECT_EQ(text[37], static_cast<char>(0x55));
}

TEST_F(RegisterDeviceTests, FillsGUIDTextForLowByteValues) {
  const char guid[SUPLA_GUID_SIZE] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

  expectGUIDText(guid, "00010203-0405-0607-0809-0A0B0C0D0E0F");
}

TEST_F(RegisterDeviceTests, FillsGUIDTextForHighBitByteValues) {
  const char guid[SUPLA_GUID_SIZE] = {
      static_cast<char>(0x80), static_cast<char>(0xFF),
      static_cast<char>(0x81), static_cast<char>(0xFE),
      static_cast<char>(0x90), static_cast<char>(0xA0),
      static_cast<char>(0xB0), static_cast<char>(0xC0),
      static_cast<char>(0xD0), static_cast<char>(0xE0),
      static_cast<char>(0xF0), static_cast<char>(0x88),
      static_cast<char>(0x99), static_cast<char>(0xAA),
      static_cast<char>(0xBB), static_cast<char>(0xCC)};

  expectGUIDText(guid, "80FF81FE-90A0-B0C0-D0E0-F08899AABBCC");
}

TEST_F(RegisterDeviceTests, ReturnsChannelStructureForValidIndex) {
  Supla::Channel channel;

  EXPECT_NE(Supla::RegisterDevice::getChannelPtr_D(0), nullptr);
  EXPECT_NE(Supla::RegisterDevice::getChannelPtr_E(0), nullptr);
}

TEST_F(RegisterDeviceTests, RejectsOutOfRangeAndNegativeIndexes) {
  Supla::Channel channel;
  ASSERT_EQ(Supla::RegisterDevice::getChannelCount(), 1);

  for (int index : {-1, -2, 1}) {
    EXPECT_EQ(Supla::RegisterDevice::getChannelPtr_D(index), nullptr);
    EXPECT_EQ(Supla::RegisterDevice::getChannelPtr_E(index), nullptr);
  }
}

TEST_F(RegisterDeviceTests, ReturnsNullWhenChannelListIsShorterThanCount) {
  Supla::Channel channel;
  Supla::RegisterDevice::addChannel(1);

  ASSERT_EQ(Supla::RegisterDevice::getChannelCount(), 2);
  EXPECT_EQ(Supla::RegisterDevice::getChannelPtr_D(1), nullptr);
  EXPECT_EQ(Supla::RegisterDevice::getChannelPtr_E(1), nullptr);
}

}  // namespace
