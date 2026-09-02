// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/device/notifications.h>
#include <supla_srpc_layer_mock.h>

using Supla::Notification;
using ::testing::_;
using ::testing::Return;

TEST(Notification, IsNotificationUsed) {
  EXPECT_FALSE(Notification::IsNotificationUsed());

  Notification notif;

  EXPECT_TRUE(Notification::IsNotificationUsed());
}

TEST(Notification, RegisterNotification) {
  ASSERT_FALSE(Notification::IsNotificationUsed());

  // create explicitly notifican, so it will be destroyed at the end of test
  Notification notif;

  EXPECT_TRUE(Notification::RegisterNotification(0, false, false, false));
  EXPECT_FALSE(Notification::RegisterNotification(0, false, false, false));
  EXPECT_FALSE(Notification::RegisterNotification(0, true, false, false));
  EXPECT_FALSE(Notification::RegisterNotification(0));

  EXPECT_TRUE(Notification::RegisterNotification(1, true, false, false));
  EXPECT_FALSE(Notification::RegisterNotification(1, true, false, false));

  EXPECT_TRUE(Notification::RegisterNotification(2, false, true, false));
  EXPECT_FALSE(Notification::RegisterNotification(2, false, true, false));

  EXPECT_TRUE(Notification::RegisterNotification(3, false, false, true));
  EXPECT_FALSE(Notification::RegisterNotification(3, false, false, true));

  EXPECT_TRUE(Notification::RegisterNotification(4, false, false, false));
  EXPECT_FALSE(Notification::RegisterNotification(4, false, false, false));
}

TEST(Notification, Send) {
  ASSERT_FALSE(Notification::IsNotificationUsed());

  Notification notif;
  SuplaSrpcLayerMock srpcMock;

  EXPECT_CALL(srpcMock, sendRegisterNotification(_)).Times(4);

  EXPECT_TRUE(Notification::RegisterNotification(0));
  EXPECT_TRUE(Notification::RegisterNotification(1, true, false, false));
  EXPECT_TRUE(Notification::RegisterNotification(2, false, true, false));
  EXPECT_TRUE(Notification::RegisterNotification(3, false, false, true));

  // send fails because of lack of SRPC configured
  EXPECT_FALSE(Notification::Send(0));
  EXPECT_FALSE(Notification::Send(1));
  EXPECT_FALSE(Notification::Send(2));
  EXPECT_FALSE(Notification::Send(3));

  notif.onRegistered(&srpcMock);

  EXPECT_CALL(srpcMock, sendNotification(0, _, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(srpcMock, sendNotification(1, _, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(srpcMock, sendNotification(2, _, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(srpcMock, sendNotification(3, _, _, _))
      .Times(1)
      .WillOnce(Return(true));

  // send succed because of lack of SRPC configured
  EXPECT_TRUE(Notification::Send(0));
  EXPECT_TRUE(Notification::Send(1));
  EXPECT_TRUE(Notification::Send(2));
  EXPECT_TRUE(Notification::Send(3));
  EXPECT_FALSE(Notification::Send(4));
}

