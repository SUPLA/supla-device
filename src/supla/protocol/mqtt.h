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

#ifndef SRC_SUPLA_PROTOCOL_MQTT_H_
#define SRC_SUPLA_PROTOCOL_MQTT_H_

#include <supla/storage/config.h>
#include <supla-common/proto.h>
#include <supla/uptime.h>
#include <supla/protocol/mqtt_topic.h>
#include <supla/element.h>

#include "protocol_layer.h"

// max client id length limit in mqtt 3.1
#define MQTT_CLIENTID_MAX_SIZE 23
#define MQTT_MAX_PAYLOAD_LEN 50

namespace Supla {

namespace Protocol {

class Mqtt : public ProtocolLayer {
 public:
  explicit Mqtt(SuplaDeviceClass *sdc);
  ~Mqtt();

  void onInit() override;
  bool onLoadConfig() override;
  bool verifyConfig() override;
  bool isEnabled() override;
//  void disconnect() override;
//  void iterate(uint64_t _millis) override;
  bool isNetworkRestartRequested() override;
  uint32_t getConnectionFailTime() override;
  bool isConnectionError() override;
  bool isConnecting() override;
  void publish(const char *topic,
               const char *payload,
               int qos = -1,
               int retain = -1,
               bool ignorePrefix = false);
  void publishInt(const char *topic,
               int payload,
               int qos = -1,
               int retain = -1);
  void publishBool(const char *topic,
               bool payload,
               int qos = -1,
               int retain = -1);
  void publishDouble(const char *topic,
               double payload,
               int qos = -1,
               int retain = -1);
  void publishChannelState(int channel);
  void subscribeChannel(int channel);
  void subscribe(const char *topic, int qos = -1);
  void notifyChannelChange(int channel) override;
  bool isUpdatePending() override;

  bool processData(const char *topic, const char *payload);

 protected:
  void generateClientId(char result[MQTT_CLIENTID_MAX_SIZE]);
  void generateObjectId(char result[30], int channelNumber, int subId);
  MqttTopic getHADiscoveryTopic(const char *sensor, char *objectId);
  void publishDeviceStatus(bool onRegistration = false);
  void publishHADiscovery(int channel);
  void publishHADiscoveryRelay(Supla::Element *);
  void publishHADiscoveryThermometer(Supla::Element *);
  void publishHADiscoveryHumidity(Supla::Element *);
  virtual void publishImp(const char *topic,
                          const char *payload,
                          int qos,
                          bool retain) = 0;
  virtual void subscribeImp(const char *topic, int qos) = 0;

  bool isPayloadOn(const char *);

  char server[SUPLA_SERVER_NAME_MAXSIZE] = {};
  int32_t port = -1;
  char user[MQTT_USERNAME_MAX_SIZE] = {};
  char password[MQTT_PASSWORD_MAX_SIZE] = {};
  char hostname[32] = {};
  uint8_t qosCfg = 0;
  bool useTls = false;
  bool useAuth = true;
  bool retainCfg = false;
  bool enabled = true;
  bool connecting = false;
  bool error = false;
  char *prefix = nullptr;
  int prefixLen = 0;
  Supla::Uptime uptime;
  bool *channelChangedFlag = nullptr;
  int channelsCount = 0;
};
}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_MQTT_H_
