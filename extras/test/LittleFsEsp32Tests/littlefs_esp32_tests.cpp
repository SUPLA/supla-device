// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <LittleFS.h>
#include <esp_littlefs.h>
#include <supla/storage/littlefs_config.h>

#include <array>

class LittleFsEsp32Tests : public ::testing::Test {
 protected:
  void SetUp() override {
    LittleFS.reset();
    // ESP32 begin() defaults to formatOnFail=false.
    LittleFS.setConfig(LittleFSConfig(false));
    lastLittleFsLabel = nullptr;
  }
};

TEST_F(LittleFsEsp32Tests, QueriesArduinoPartitionAndReusesItsMount) {
  Supla::LittleFsConfig config;
  std::array<char, 60> blob = {};
  ASSERT_TRUE(config.setBlob("blob", blob.data(), blob.size()));
  ASSERT_EQ(LittleFS.beginCalls, 1);
  ASSERT_TRUE(config.getBlob("blob", blob.data(), blob.size()));
  EXPECT_STREQ(lastLittleFsLabel, "spiffs");
  EXPECT_EQ(LittleFS.beginCalls, 1);
  EXPECT_EQ(LittleFS.beginInvocations, 1);
  EXPECT_EQ(LittleFS.endCalls, 0);
  EXPECT_EQ(LittleFS.openFiles, 0);
}

TEST_F(LittleFsEsp32Tests, MountFailureNeverFormatsDuringCertificateAccess) {
  Supla::LittleFsConfig config;
  std::array<char, 60> blob = {};
  ASSERT_TRUE(config.setBlob("blob", blob.data(), blob.size()));
  LittleFS.end();
  LittleFS.beginResult = false;
  EXPECT_FALSE(config.setMqttCA("certificate"));
  EXPECT_EQ(LittleFS.formatCalls, 0);
  EXPECT_TRUE(LittleFS.exists("/supla/blob"));
  LittleFS.beginResult = true;
  EXPECT_TRUE(config.getBlob("blob", blob.data(), blob.size()));
  EXPECT_EQ(LittleFS.beginCalls, 3);
  EXPECT_STREQ(lastLittleFsLabel, "spiffs");
}
