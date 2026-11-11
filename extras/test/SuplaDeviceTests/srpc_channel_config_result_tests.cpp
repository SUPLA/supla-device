// SPDX-FileCopyrightText: 2026 sd4linux-devireg contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/channel_element.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>

using ::testing::_;
using ::testing::NiceMock;

namespace {

class ConfigResultElement : public Supla::ChannelElement {
 public:
  MOCK_METHOD(uint8_t, handleChannelConfig,
              (TSD_ChannelConfig *, bool), (override));
  MOCK_METHOD(uint8_t, handleWeeklySchedule,
              (TSD_ChannelConfig *, bool, bool), (override));
};

}  // namespace

TEST(SuplaSrpcChannelConfigResultTests, RepliesPreserveRequestTypeAndChannel) {
  SimpleTime time;
  Supla::RegisterDevice::resetToDefaults();
  NiceMock<SrpcMock> srpc;
  SuplaDeviceClass device;
  Supla::Protocol::SuplaSrpc protocol(&device);
  NiceMock<ConfigResultElement> element;
  ASSERT_TRUE(element.getChannel()->setChannelNumber(4));

  for (auto type : {SUPLA_CONFIG_TYPE_DEFAULT,
                    SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE,
                    SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE}) {
    for (auto status : {SUPLA_CONFIG_RESULT_TRUE,
                        SUPLA_CONFIG_RESULT_DATA_ERROR,
                        SUPLA_RESULTCODE_CHANNELNOTFOUND}) {
      TSD_ChannelConfig request = {};
      request.ChannelNumber = status == SUPLA_RESULTCODE_CHANNELNOTFOUND
                                  ? 19 : 4;
      request.ConfigType = type;
      EXPECT_CALL(srpc, srpc_getdata(_, _, _))
          .WillOnce([&request](void *, TsrpcReceivedData *rd,
                               unsigned _supla_int_t) {
            *rd = {};
            rd->call_id = SUPLA_SD_CALL_SET_CHANNEL_CONFIG;
            rd->data.sds_set_channel_config_request = &request;
            return SUPLA_RESULT_TRUE;
          });
      if (status != SUPLA_RESULTCODE_CHANNELNOTFOUND) {
        if (type == SUPLA_CONFIG_TYPE_DEFAULT) {
          EXPECT_CALL(element, handleChannelConfig(&request, false))
              .WillOnce(::testing::Return(status));
        } else {
          bool alternative = type == SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE;
          EXPECT_CALL(element, handleWeeklySchedule(
                                   &request, alternative, false))
              .WillOnce(::testing::Return(status));
        }
      }
      EXPECT_CALL(srpc, setChannelConfigResult(_))
          .WillOnce([&request, status](TSDS_SetChannelConfigResult *result) {
            EXPECT_EQ(result->Result, status);
            EXPECT_EQ(result->ConfigType, request.ConfigType);
            EXPECT_EQ(result->ChannelNumber, request.ChannelNumber);
            return 1;
          });
      EXPECT_CALL(srpc, srpc_rd_free(_));
      Supla::messageReceived(nullptr, 0, 0, &protocol, 27);
      ::testing::Mock::VerifyAndClearExpectations(&srpc);
      ::testing::Mock::VerifyAndClearExpectations(&element);
    }
  }
}
