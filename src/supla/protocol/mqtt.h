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

// https://developers.home-assistant.io/docs/core/entity/sensor/
enum HAStateClass {
  HAStateClass_None,
  HAStateClass_Measurement,
  HAStateClass_Total,
  HAStateClass_TotalIncreasing
};

// https://www.home-assistant.io/integrations/sensor/#device-class
// Not all devices classes are implemented here. Only those which we use are
// present. Add more if needed.
// Also not all parameters used in Supla have corresponding device class in
// HA, so in such case we don't set the device class.
enum HADeviceClass {
  HADeviceClass_None,
  HADeviceClass_ApparentPower,
  HADeviceClass_Current,
  HADeviceClass_Energy,
  HADeviceClass_Frequency,
  HADeviceClass_PowerFactor,
  HADeviceClass_Power,
  HADeviceClass_ReactivePower,
  HADeviceClass_Voltage,
  HADeviceClass_Outlet,
  HADeviceClass_Gate,
  HADeviceClass_Door,
  HADeviceClass_Garage,
  HADeviceClass_Moisture,
  HADeviceClass_Window
};

class Mqtt : public ProtocolLayer {
 public:
  explicit Mqtt(SuplaDeviceClass *sdc);
  ~Mqtt();

  void onInit() override;
  bool onLoadConfig() override;
  bool verifyConfig() override;
  bool isEnabled() override;
//  void disconnect() override;
//  void iterate(uint32_t _millis) override;
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
  void publishOnOff(const char *topic, bool payload, int qos, int retain);
  void publishOpenClosed(const char *topic, bool payload, int qos, int retain);
  void publishDouble(const char *topic,
               double payload,
               int qos = -1,
               int retain = -1,
               int precision = 2);
  void publishColor(const char *topic,
      uint8_t red,
      uint8_t green,
      uint8_t blue,
      int qos = -1,
      int retain = -1);
  void publishChannelState(int channel);
  void publishExtendedChannelState(int channel);
  void subscribeChannel(int channel);
  void subscribe(const char *topic, int qos = -1);
  bool isUpdatePending() override;
  bool isRegisteredAndReady() override;
  void notifyConfigChange(int channelNumber) override;

  void sendActionTrigger(uint8_t channelNumber, uint32_t actionId) override;
  void sendChannelValueChanged(uint8_t channelNumber, char *value,
      unsigned char offline, uint32_t validityTimeSec) override;
  void sendExtendedChannelValueChanged(uint8_t channelNumber,
    TSuplaChannelExtendedValue *value) override;

  bool processData(const char *topic, const char *payload);
  void processRelayRequest(const char *topic,
                           const char *payload,
                           Supla::Element *element);
  void processRGBWRequest(const char *topic,
                          const char *payload,
                          Supla::Element *element);
  void processRGBRequest(const char *topic,
                         const char *payload,
                         Supla::Element *element);
  void processDimmerRequest(const char *topic,
                            const char *payload,
                            Supla::Element *element);
  void processHVACRequest(const char *topic,
                          const char *payload,
                          Supla::Element *element);

 protected:
  void generateClientId(char result[MQTT_CLIENTID_MAX_SIZE]);
  void generateObjectId(char result[30], int channelNumber, int subId);
  MqttTopic getHADiscoveryTopic(const char *sensor, char *objectId);
  void publishDeviceStatus(bool onRegistration = false);
  void publishHADiscovery(int channel);
  void publishHADiscoveryRelay(Supla::Element *);
  void publishHADiscoveryRelayImpulse(Supla::Element *);
  void publishHADiscoveryThermometer(Supla::Element *);
  void publishHADiscoveryHumidity(Supla::Element *);
  void publishHADiscoveryActionTrigger(Supla::Element *);
  void publishHADiscoveryEM(Supla::Element *);
  void publishHADiscoveryRGB(Supla::Element *);
  void publishHADiscoveryDimmer(Supla::Element *);
  void publishHADiscoveryHVAC(Supla::Element *);
  void publishHADiscoveryBinarySensor(Supla::Element *);

  // parameterName has to be ASCII string with small caps and underscores
  // between words i.e. "total_forward_active_energy".
  // Name of parameter will be generated in following way:
  // "Total forward active energy"
  void publishHADiscoveryEMParameter(
    Supla::Element *element, int parameterId, const char *parameterName,
    const char *units, Supla::Protocol::HAStateClass stateClass,
    Supla::Protocol::HADeviceClass deviceClass);
  const char *getActionTriggerType(uint8_t actionIdx);
  bool isActionTriggerEnabled(Supla::Channel *ch, uint8_t actionIdx);
  virtual void publishImp(const char *topic,
                          const char *payload,
                          int qos,
                          bool retain) = 0;
  virtual void subscribeImp(const char *topic, int qos) = 0;
  const char *getStateClassStr(Supla::Protocol::HAStateClass stateClass);
  const char *getDeviceClassStr(Supla::Protocol::HADeviceClass deviceClass);

  const char *getRelayChannelName(int channelFunction) const;
  const char *getBinarySensorChannelName(int channelFunction) const;

  bool isPayloadOn(const char *);
  bool isOpenClosedBinarySensorFunction(int channelFunction) const;

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
  bool connected = false;
  bool error = false;
  char *prefix = nullptr;
  int prefixLen = 0;
  Supla::Uptime uptime;
  int channelsCount = 0;
  // Button number is incremented on each publishHADiscoveryActionTrigger call
  // and it is reset on publishDeviceStatus. So we publish button numbers
  // starting from 1 and incrementing on each ActionTrigger channel found
  // in current setup.
  // It is important to call publishDeviceStatus first, then to call
  // publishHADiscoveryActionTrigger for each AT channel.
  int buttonNumber = 0;
  uint8_t configChangedBit[8] = {};
};
}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_MQTT_H_
