// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_NOTIFICATIONS_H_
#define SRC_SUPLA_DEVICE_NOTIFICATIONS_H_

#include <supla-common/proto.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/element.h>

#define SUPLA_NOTIF_MAX 50

namespace Supla {

class Notification : public Element {
 public:
  Notification();
  virtual ~Notification();

  // Register a notification locally at any time.
  // Configurations present when SUPLA device registration starts are sent
  // once it succeeds. Configurations added during or after it wait for the
  // next registration. Create the channel before registering its notification.
  // context - -1 device, 0..255 channel number
  // titleSetByDevice - true if title is set by device, false if managed by
  // server
  // messageSetByDevice - true if message is set by device, false if managed by
  // server
  // soundSetByDevice - true if sound is set by device, false if managed by
  // server
  static bool RegisterNotification(int16_t context,
                                   bool titleSetByDevice = true,
                                   bool messageSetByDevice = true,
                                   bool soundSetByDevice = false);

  // Send a notification
  // The notification configuration must have been submitted by onRegistered().
  // context - -1 device, 0..255 channel id
  // title - title of the notification
  // message - message of the notification
  // soundId - sound id
  // If any of above fields was registered as not manged by device, then this
  // field will be ignored
  // Return true if the send request was submitted to SRPC.
  // Return false if the configuration is unavailable or the connection is
  // not ready.
  static bool Send(int16_t context,
                   const char *title = nullptr,
                   const char *message = nullptr,
                   int soundId = 0);

  static bool SendF(int16_t context, const char* title,  const char *fmt, ...);

  static bool IsNotificationUsed();

  // Called by SRPC just before sending the device registration request.
  static void OnRegistrationStarted();
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc = nullptr) override;

 private:
  static Notification* GetInstance();
  static Notification* instance;

  bool registerNotification(int16_t context,
                            bool titleSetByDevice,
                            bool messageSetByDevice,
                            bool soundSetByDevice);
  bool send(int16_t context, const char *title, const char *message,
           int soundId);
  TDS_RegisterPushNotification* getContextConfig(int16_t context);

  void setSrpc(Supla::Protocol::SuplaSrpc *srpc);

  TDS_RegisterPushNotification notifications[SUPLA_NOTIF_MAX] = {};
  uint8_t notificationCount = 0;
  // Count captured for this registration. Send is enabled once it succeeds.
  uint8_t registeredNotificationCount = 0;
  Supla::Protocol::SuplaSrpc *srpc = nullptr;
};
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_NOTIFICATIONS_H_
