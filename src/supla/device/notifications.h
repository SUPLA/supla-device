/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

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

  // Register a notification
  // All notifications should be registered before SuplaDevice.begin()
  // context - -1 device, 0..255 channel id
  // titleSetByDevice - true if title is set by device, false if managed by
  // server
  // messageSetByDevice - true if message is set by device, false if managed by
  // server
  // soundSetByDevice - true if sound is set by device, false if managed by
  // server
  static bool RegisterNotification(int context,
                                   bool titleSetByDevice = true,
                                   bool messageSetByDevice = true,
                                   bool soundSetByDevice = false);

  // Send a notification
  // Notification has to be registered earlier.
  // context - -1 device, 0..255 channel id
  // title - title of the notification
  // message - message of the notification
  // soundId - sound id
  // If any of above fields was registered as not manged by device, then this
  // field will be ignored
  // Return true if notification was sent
  // Return false if notification was not sent (i.e. it wasn't registered,
  // or Supla server connection is not ready)
  static bool Send(int context,
                   const char *title = nullptr,
                   const char *message = nullptr,
                   int soundId = 0);

  static bool SendF(int context, const char* title,  const char *fmt, ...);

  static bool IsNotificationUsed();

  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc = nullptr) override;

 private:
  static Notification* GetInstance();
  static Notification* instance;

  bool registerNotification(int context,
                            bool titleSetByDevice,
                            bool messageSetByDevice,
                            bool soundSetByDevice);
  bool send(int context, const char *title, const char *message,
           int soundId);
  TDS_RegisterPushNotification* getContextConfig(int context);

  void setSrpc(Supla::Protocol::SuplaSrpc *srpc);

  TDS_RegisterPushNotification notifications[SUPLA_NOTIF_MAX] = {};
  int notificationCount = 0;
  Supla::Protocol::SuplaSrpc *srpc = nullptr;
  bool blockNewRegistration = false;
};
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_NOTIFICATIONS_H_
