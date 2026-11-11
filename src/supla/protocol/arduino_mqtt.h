// SPDX-FileCopyrightText: Goral64
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_PROTOCOL_ARDUINO_MQTT_H_
#define SRC_SUPLA_PROTOCOL_ARDUINO_MQTT_H_

#ifdef ARDUINO_ARCH_ESP8266
#include <supla/protocol/mqtt.h>

namespace Supla {

class Client;

namespace Protocol {

// MQTT 3.1.1 client on top of Supla::Client (TCP or TLS), QoS 0 and 1.
class ArduinoMqtt : public Mqtt {
 public:
  explicit ArduinoMqtt(SuplaDeviceClass *sdc);
  virtual ~ArduinoMqtt();

  void onInit() override;
  void disconnect() override;
  bool iterate(uint32_t _millis) override;
  ConnectionError getConnectionError() const override;

 protected:
  void publishImp(const char *topic,
                  const char *payload,
                  int qos,
                  bool retain) override;
  void subscribeImp(const char *topic, int qos) override;
  void unsubscribeImp(const char *topic) override;

 private:
  enum class State : uint8_t { Idle, WaitConnAck, Connected };

  bool connectToBroker(uint32_t _millis);
  void closeConnection();
  void setConnectionError(ConnectionError newError, const char *message);
  bool writeFixedHeader(uint8_t type, uint32_t remainingLength);
  bool writeString(const char *str, uint16_t len);
  bool writeUInt16(uint16_t value);
  uint16_t nextPacketId();
  void readIncoming(uint32_t _millis);
  void handlePacket();
  void handlePublish();

  static constexpr uint16_t RX_BUFFER_SIZE =
      MAX_TOPIC_LEN + MQTT_MAX_PAYLOAD_LEN + 4;

  Supla::Client *client = nullptr;
  char *mqttCaCert = nullptr;
  State state = State::Idle;
  bool enterRegisteredAndReady = false;
  ConnectionError connectionError = ConnectionError::NONE;
  const char *lastMessage = nullptr;
  uint32_t lastConnectAttemptMs = 0;
  uint32_t connectStartMs = 0;
  uint32_t lastRxMs = 0;
  uint32_t lastTxMs = 0;
  uint32_t keepAliveMs = 0;
  uint32_t lastStatusUpdateSec = 0;
  uint16_t packetId = 0;
  bool firstConnectAttempt = true;

  // incoming packet parser
  uint8_t rxHeader = 0;
  uint8_t rxLengthBytes = 0;
  uint32_t rxRemaining = 0;
  uint32_t rxLength = 0;
  uint16_t rxPos = 0;
  bool rxInBody = false;
  char rxBuffer[RX_BUFFER_SIZE] = {};
};

}  // namespace Protocol
}  // namespace Supla

#endif  // ARDUINO_ARCH_ESP8266
#endif  // SRC_SUPLA_PROTOCOL_ARDUINO_MQTT_H_
