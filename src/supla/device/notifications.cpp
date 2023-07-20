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

#include "notifications.h"

#include <supla/log_wrapper.h>
#include <stdio.h>
#include <stdarg.h>

using Supla::Notification;

Notification* Notification::instance = nullptr;

Notification::Notification() {
  if (instance != nullptr) {
    SUPLA_LOG_ERROR("Notification instance already created!");
    return;
  }
  instance = this;
}

Notification::~Notification() {
  instance = nullptr;
}

Notification* Notification::GetInstance() {
  if (instance == nullptr) {
    instance = new Notification();
  }
  return instance;
}

bool Notification::IsNotificationUsed() {
  return instance != nullptr;
}

bool Notification::RegisterNotification(int context,
                                        bool titleSetByDevice,
                                        bool messageSetByDevice,
                                        bool soundSetByDevice) {
  return GetInstance()->registerNotification(
      context, titleSetByDevice, messageSetByDevice, soundSetByDevice);
}

bool Notification::Send(int context,
                   const char *title,
                   const char *message,
                   int soundId) {
  if (!IsNotificationUsed()) {
    SUPLA_LOG_WARNING(
        "Notification: can't send - notifications not registered");
    return false;
  }
  return GetInstance()->send(context, title, message, soundId);
}

bool Notification::SendF(int context,
                        const char *title,
                        const char *format,
                        ...) {
  if (format == nullptr) {
    return false;
  }
  char message[SUPLA_PN_BODY_MAXSIZE] = {};

  va_list args;
  va_start(args, format);
  vsnprintf(message, SUPLA_PN_BODY_MAXSIZE, format, args);
  auto result = Send(context, title, message, -1);
  va_end(args);
  return result;
}

bool Notification::registerNotification(int context,
                                        bool titleSetByDevice,
                                        bool messageSetByDevice,
                                        bool soundSetByDevice) {
  if (blockNewRegistration) {
    SUPLA_LOG_WARNING(
        "Notification: can't register anymore - blockNewRegistration is set");
    return false;
  }

  int i = 0;
  for (; i < notificationCount; i++) {
    if (notifications[i].Context == context) {
      SUPLA_LOG_WARNING(
          "Notification: can't register - notification already registered for "
          "context %d",
          context);
      return false;
    }
  }
  if (i < SUPLA_NOTIF_MAX) {
    notifications[i].Context = context;
    if (!titleSetByDevice) {
      notifications[i].ServerManagedFields |= PN_SERVER_MANAGED_TITLE;
    }
    if (!messageSetByDevice) {
      notifications[i].ServerManagedFields |= PN_SERVER_MANAGED_BODY;
    }
    if (!soundSetByDevice) {
      notifications[i].ServerManagedFields |= PN_SERVER_MANAGED_SOUND;
    }
    notificationCount++;
    SUPLA_LOG_INFO("Notification: registered - context %d", context);
    return true;
  }
  SUPLA_LOG_WARNING(
      "Notification: can't register - notification count exceeded");
  return false;
}

bool Notification::send(int context,
                        const char *title,
                        const char *message,
                        int soundId) {
  if (srpc == nullptr) {
    SUPLA_LOG_WARNING("Notification: can't send - srpc not set");
    return false;
  }

  auto config = getContextConfig(context);

  if (config == nullptr) {
    SUPLA_LOG_WARNING(
      "Notification: can't send - context %d not registered", context);
    return false;
  }

  if (config->ServerManagedFields & PN_SERVER_MANAGED_TITLE) {
    title = nullptr;
  }
  if (config->ServerManagedFields & PN_SERVER_MANAGED_BODY) {
    message = nullptr;
  }
  if (config->ServerManagedFields & PN_SERVER_MANAGED_SOUND) {
    soundId = -1;
  }

  SUPLA_LOG_DEBUG("Notification: sending - context %d", context);
  return srpc->sendNotification(context, title, message, soundId);
}

TDS_RegisterPushNotification* Notification::getContextConfig(int context) {
  for (int i = 0; i < notificationCount; i++) {
    if (notifications[i].Context == context) {
      return &notifications[i];
    }
  }
  return nullptr;
}

void Notification::setSrpc(Supla::Protocol::SuplaSrpc *srpc) {
  this->srpc = srpc;
}

void Notification::onRegistered(Supla::Protocol::SuplaSrpc *srpc) {
  blockNewRegistration = true;
  setSrpc(srpc);

  for (int i = 0; i < notificationCount; i++) {
    SUPLA_LOG_DEBUG("Notification: sending registration for context %d",
                    notifications[i].Context);
    srpc->sendRegisterNotification(&notifications[i]);
  }
}
