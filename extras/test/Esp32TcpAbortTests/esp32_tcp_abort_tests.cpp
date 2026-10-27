// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <fake_lwip.h>
#include <lwip/priv/tcp_priv.h>
#include <supla/network/esp32_tcp_abort.h>

#include <string>

class Esp32TcpAbortTests : public ::testing::Test {
 protected:
  void SetUp() override {
    fakeSocket = &socket;
    fakeResult = ERR_OK;
    lookupCount = apiCallCount = abortCount = 0;
    coreLocked = inTcpipTask = 0;
    lastFd = -1;
    lastLog[0] = 0;
    pcb.state = ESTABLISHED;
    pcb.callback_arg = &conn;
    conn.type = NETCONN_TCP;
    conn.pcb.tcp = &pcb;
    socket.conn = &conn;
  }
  tcp_pcb pcb = {};
  netconn conn = {};
  lwip_sock socket = {};
};

TEST_F(Esp32TcpAbortTests, NegativeFdDoesNotEnterCoreOrLookupSocket) {
  Supla::abortTcpConnection(-1);
  EXPECT_EQ(apiCallCount, 0);
  EXPECT_EQ(lookupCount, 0);
  EXPECT_EQ(abortCount, 0);
  EXPECT_STREQ(lastLog, "");
}

TEST_F(Esp32TcpAbortTests, SynchronizationFailureLeavesSocketForNormalClose) {
  fakeResult = ERR_MEM;
  Supla::abortTcpConnection(12);
  EXPECT_EQ(apiCallCount, 1);
  EXPECT_EQ(lookupCount, 0);
  EXPECT_EQ(abortCount, 0);
  EXPECT_EQ(conn.pcb.tcp, &pcb);
  EXPECT_NE(std::string(lastLog).find("synchronization failed"),
            std::string::npos);
}

TEST_F(Esp32TcpAbortTests, SocketLookupFailureIsSafe) {
  fakeSocket = nullptr;
  Supla::abortTcpConnection(LWIP_SOCKET_OFFSET);
  EXPECT_EQ(lastFd, LWIP_SOCKET_OFFSET);
  EXPECT_EQ(lookupCount, 1);
  EXPECT_EQ(abortCount, 0);
  EXPECT_STREQ(lastLog, "");
}

TEST_F(Esp32TcpAbortTests, MissingConnectionAndPcbAreSafe) {
  socket.conn = nullptr;
  Supla::abortTcpConnection(12);
  socket.conn = &conn;
  conn.pcb.tcp = nullptr;
  Supla::abortTcpConnection(12);
  EXPECT_EQ(abortCount, 0);
  EXPECT_STREQ(lastLog, "");
}

TEST_F(Esp32TcpAbortTests, NonTcpAndListenerAreNotAborted) {
  conn.type = NETCONN_UDP;
  Supla::abortTcpConnection(12);
  conn.type = NETCONN_TCP;
  pcb.state = LISTEN;
  Supla::abortTcpConnection(12);
  EXPECT_EQ(abortCount, 0);
  EXPECT_STREQ(lastLog, "");
}

TEST_F(Esp32TcpAbortTests, AbortsIpv4Ipv6AndConnectingSocketsInsideCore) {
  for (int type : {NETCONN_TCP, NETCONN_TCP_IPV6}) {
    for (auto state : {SYN_SENT, ESTABLISHED, FIN_WAIT_1}) {
      conn.type = type;
      conn.pcb.tcp = &pcb;
      pcb.state = state;
      Supla::abortTcpConnection(12);
      EXPECT_EQ(conn.pcb.tcp, nullptr);
      EXPECT_EQ(coreLocked, 0);
      EXPECT_EQ(inTcpipTask, 0);
    }
  }
  EXPECT_EQ(abortCount, 6);
}

TEST_F(Esp32TcpAbortTests, RepeatedAbortAfterErrorCallbackDoesNotTouchOldPcb) {
  Supla::abortTcpConnection(12);
  Supla::abortTcpConnection(12);
  EXPECT_EQ(abortCount, 1);
  EXPECT_EQ(conn.pcb.tcp, nullptr);
}

TEST_F(Esp32TcpAbortTests, ReportsQueuedPayloadBytesOutsideCore) {
  tcp_seg tail = {nullptr, 13};
  tcp_seg unacked = {&tail, 12};
  tcp_seg unsent = {nullptr, 7};
  pcb.unacked = &unacked;
  pcb.unsent = &unsent;
  Supla::abortTcpConnection(12);
  EXPECT_EQ(abortCount, 1);
  EXPECT_NE(std::string(lastLog).find("queued data bytes 32"),
            std::string::npos);
}

TEST_F(Esp32TcpAbortTests, OutOfRangeDescriptorsNeverEnterCore) {
  for (int fd : {-1, LWIP_SOCKET_OFFSET - 1,
                 LWIP_SOCKET_OFFSET + NUM_SOCKETS,
                 LWIP_SOCKET_OFFSET + NUM_SOCKETS + 1}) {
    Supla::abortTcpConnection(fd);
  }
  EXPECT_EQ(apiCallCount, 0);
  EXPECT_EQ(lookupCount, 0);
  EXPECT_EQ(abortCount, 0);
  EXPECT_STREQ(lastLog, "");
}

TEST_F(Esp32TcpAbortTests, BothSocketRangeEndpointsAreValid) {
  for (int fd : {LWIP_SOCKET_OFFSET, LWIP_SOCKET_OFFSET + NUM_SOCKETS - 1}) {
    conn.pcb.tcp = &pcb;
    Supla::abortTcpConnection(fd);
    EXPECT_EQ(lastFd, fd);
  }
  EXPECT_EQ(apiCallCount, 2);
  EXPECT_EQ(abortCount, 2);
}

TEST_F(Esp32TcpAbortTests, RepeatedAbortDoesNotLogAgain) {
  Supla::abortTcpConnection(12);
  ASSERT_NE(lastLog[0], 0);
  lastLog[0] = 0;
  Supla::abortTcpConnection(12);
  EXPECT_STREQ(lastLog, "");
  EXPECT_EQ(abortCount, 1);
}
