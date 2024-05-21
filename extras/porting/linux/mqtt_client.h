/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef EXTRAS_PORTING_LINUX_MQTT_CLIENT_H_
#define EXTRAS_PORTING_LINUX_MQTT_CLIENT_H_

#include <fcntl.h>
#include <mqtt.h>
#include <mqtt_pal.h>
#include <netdb.h>
#include <pthread.h>
#include <supla/log_wrapper.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <array>

using std::vector;
using std::string;

struct reconnect_state_t {
  std::string hostname;
  int port;
  bool useSSL = false;
  bool verifyCA = false;
  std::string fileCA;
  std::string username;
  std::string password;
  std::string clientName;
  std::array<uint8_t, 8192> sendbuf;
  std::array<uint8_t, 2048> recvbuf;
  std::unordered_map<std::string, std::string> topics;
};

int mqtt_client_init(std::string addr,
                     int port,
                     std::string username,
                     std::string password,
                     std::string client_name,
                     const std::unordered_map<std::string, std::string>& topics,
                     void (*publish_response_callback)(
                         void** state, struct mqtt_response_publish* publish));

void mqtt_client_publish(const char* topic,
                         const char* payload,
                         char retain,
                         char qos);

void mqtt_client_free();

#endif  // EXTRAS_PORTING_LINUX_MQTT_CLIENT_H_
