// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <gtest/gtest.h>
#include <supla/protocol/mqtt.h>

class MqttConfigChangeTest : public Supla::Protocol::Mqtt {
 public:
  explicit MqttConfigChangeTest(SuplaDeviceClass *sdc) : Mqtt(sdc) {}

  void disconnect() override {}
  bool iterate(uint32_t) override { return false; }
  void publishImp(const char *, const char *, int, bool) override {}
  void subscribeImp(const char *, int) override {}

  bool isConfigChanged(int channelNumber) const {
    if (channelNumber < 0 || channelNumber >= SUPLA_CHANNELMAXCOUNT) {
      return false;
    }
    return configChangedBit[channelNumber / 8] &
           (1U << (channelNumber % 8));
  }

  bool isConfigChangeBitsetEmpty() const {
    for (size_t i = 0; i < sizeof(configChangedBit); i++) {
      if (configChangedBit[i] != 0) {
        return false;
      }
    }
    return true;
  }

  size_t configChangeBitsetSize() const {
    return sizeof(configChangedBit);
  }
};

TEST(MqttConfigChangeTests, SupportsCompleteChannelRange) {
  SuplaDeviceClass sdc;
  MqttConfigChangeTest mqtt(&sdc);

  EXPECT_EQ(mqtt.configChangeBitsetSize(),
            (SUPLA_CHANNELMAXCOUNT + 7) / 8);

  for (int channel : {63, 64, 65, 127}) {
    mqtt.notifyConfigChange(channel);
    EXPECT_TRUE(mqtt.isConfigChanged(channel));
  }
}

TEST(MqttConfigChangeTests, IgnoresChannelsOutsideSupportedRange) {
  SuplaDeviceClass sdc;
  MqttConfigChangeTest mqtt(&sdc);

  mqtt.notifyConfigChange(-1);
  mqtt.notifyConfigChange(SUPLA_CHANNELMAXCOUNT);
  mqtt.notifyConfigChange(254);

  EXPECT_TRUE(mqtt.isConfigChangeBitsetEmpty());
}
