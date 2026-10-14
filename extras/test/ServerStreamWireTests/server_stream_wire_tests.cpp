// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <supla-common/srpc.h>
#include <algorithm>
#include <cstring>
#include <vector>
// Logging sink only; frame decoding uses the actual shared implementation.
extern "C" void supla_vlog(int, const char *) {}
namespace {
struct Wire {
  std::vector<char> bytes;
  bool accepted = false;
  int calls = 0;
  TSD_SuplaRemoteChannelState state = {};
  static int read(void *out, int count, void *context) {
    auto self = static_cast<Wire *>(context);
    const auto size = std::min(self->bytes.size(), static_cast<size_t>(count));
    memcpy(out, self->bytes.data(), size);
    self->bytes.erase(self->bytes.begin(), self->bytes.begin() + size);
    return size;
  }
  static int write(void *, int count, void *) { return count; }
  static void received(void *parser, unsigned int, unsigned int, void *context,
                        unsigned char) {
    auto self = static_cast<Wire *>(context);
    ++self->calls;
    TsrpcReceivedData data = {};
    self->accepted = srpc_getdata(parser, &data, 0) == SUPLA_RESULT_TRUE;
    if (self->accepted) {
      EXPECT_EQ(data.call_id, SUPLA_SD_CALL_REMOTE_CHANNEL_STATE);
      ASSERT_NE(data.data.sd_remote_channel_state, nullptr);
      self->state = *data.data.sd_remote_channel_state;
    }
    srpc_rd_free(&data);
  }
  void run(size_t size) {
    TSuplaDataPacket packet = {};
    memcpy(packet.tag, "SUPLA", SUPLA_TAG_SIZE);
    packet.version = 29;
    packet.rr_id = 1;
    packet.call_id = SUPLA_SD_CALL_REMOTE_CHANNEL_STATE;
    packet.data_size = size;
    TSD_SuplaRemoteChannelState value = {};
    value.ChannelId = 1001;
    value.Channel.Number = 0xff;
    value.Channel.Type = SUPLA_CHANNELTYPE_THERMOMETER;
    value.Channel.Default = SUPLA_CHANNELFNC_THERMOMETER;
    memcpy(packet.data, &value, std::min(size, sizeof(value)));
    const size_t length = sizeof(packet) - SUPLA_MAX_DATA_SIZE + size;
    auto start = reinterpret_cast<char *>(&packet);
    bytes.assign(start, start + length);
    bytes.insert(bytes.end(), {'S', 'U', 'P', 'L', 'A'});
    TsrpcParams params = {};
    params.data_read = read;
    params.data_write = write;
    params.on_remote_call_received = received;
    params.user_params = this;
    auto parser = srpc_init(&params);
    ASSERT_NE(parser, nullptr);
    srpc_set_proto_version(parser, 29);
    srpc_iterate(parser);
    srpc_free(parser);
  }
};
TEST(ServerStreamWire, ActualSrpcParserAcceptsExact40BytePayload) {
  Wire wire;
  wire.run(40);
  ASSERT_EQ(wire.calls, 1);
  ASSERT_TRUE(wire.accepted);
  EXPECT_EQ(wire.state.ChannelId, 1001);
  EXPECT_EQ(wire.state.Channel.Number, 0xff);
}
TEST(ServerStreamWire, ActualSrpcParserRejectsTruncatedAndOversizedPayload) {
  for (size_t size : {0, 1, 39, 41, 80}) {
    Wire wire;
    wire.run(size);
    ASSERT_EQ(wire.calls, 1);
    EXPECT_FALSE(wire.accepted) << size;
  }
}
}  // namespace
