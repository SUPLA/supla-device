// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/device/notifications.h>
#include <supla_srpc_layer_mock.h>

using Supla::Notification;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::StrictMock;

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

TEST(Notification, LateRegistrationIsSentOnReconnect) {
  Notification notif;
  StrictMock<SuplaSrpcLayerMock> srpcMock;

  const auto deviceConfig = Pointee(AllOf(
      Field(&TDS_RegisterPushNotification::Context, -1),
      Field(&TDS_RegisterPushNotification::ServerManagedFields,
            PN_SERVER_MANAGED_SOUND)));
  const auto channelConfig = Pointee(AllOf(
      Field(&TDS_RegisterPushNotification::Context, 7),
      Field(&TDS_RegisterPushNotification::ServerManagedFields,
            PN_SERVER_MANAGED_TITLE | PN_SERVER_MANAGED_BODY)));

  ASSERT_TRUE(Notification::RegisterNotification(-1));
  EXPECT_CALL(srpcMock, sendRegisterNotification(deviceConfig));
  Notification::OnRegistrationStarted();
  notif.onRegistered(&srpcMock);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&srpcMock));

  ASSERT_TRUE(Notification::RegisterNotification(7, false, false, true));
  EXPECT_FALSE(Notification::RegisterNotification(7));
  EXPECT_FALSE(Notification::Send(7));
  EXPECT_FALSE(Notification::SendF(7, "Title", "%s", "Body"));

  EXPECT_CALL(srpcMock, sendNotification(-1, _, _, _)).WillOnce(Return(true));
  EXPECT_TRUE(Notification::Send(-1));

  Notification::OnRegistrationStarted();
  ASSERT_TRUE(Notification::RegisterNotification(8));
  EXPECT_FALSE(Notification::Send(7));
  EXPECT_CALL(srpcMock, sendRegisterNotification(deviceConfig));
  EXPECT_CALL(srpcMock, sendRegisterNotification(channelConfig));
  notif.onRegistered(&srpcMock);
  EXPECT_FALSE(Notification::Send(8));
  EXPECT_CALL(srpcMock, sendNotification(7, nullptr, nullptr, 0))
      .WillOnce(Return(true));
  EXPECT_TRUE(Notification::Send(7));
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&srpcMock));

  EXPECT_CALL(srpcMock, sendRegisterNotification(deviceConfig));
  EXPECT_CALL(srpcMock, sendRegisterNotification(channelConfig));
  EXPECT_CALL(srpcMock, sendRegisterNotification(
      Pointee(Field(&TDS_RegisterPushNotification::Context, 8))));
  Notification::OnRegistrationStarted();
  notif.onRegistered(&srpcMock);
  EXPECT_CALL(srpcMock, sendNotification(8, _, _, _)).WillOnce(Return(true));
  EXPECT_TRUE(Notification::Send(8));
}

TEST(Notification, RegistrationLimitStillAppliesAfterRegistration) {
  Notification notif;
  StrictMock<SuplaSrpcLayerMock> srpcMock;

  notif.onRegistered(&srpcMock);
  for (int context = 0; context < SUPLA_NOTIF_MAX; context++) {
    ASSERT_TRUE(Notification::RegisterNotification(context));
  }
  EXPECT_FALSE(Notification::RegisterNotification(SUPLA_NOTIF_MAX));
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

  Notification::OnRegistrationStarted();
  notif.onRegistered(nullptr);
  EXPECT_FALSE(Notification::Send(0));
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

