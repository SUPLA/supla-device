// SPDX-FileCopyrightText: Goral64
// SPDX-License-Identifier: GPL-2.0-or-later

#include "arduino_mqtt.h"

#ifdef ARDUINO_ARCH_ESP8266
#include <Arduino.h>
#include <string.h>

#include <SuplaDevice.h>
#include <supla/log_wrapper.h>
#include <supla/network/client.h>
#include <supla/storage/config.h>

namespace {
constexpr uint8_t MQTT_CONNECT = 0x10;
constexpr uint8_t MQTT_PUBLISH = 0x30;
constexpr uint8_t MQTT_PUBACK = 0x40;
constexpr uint8_t MQTT_SUBSCRIBE = 0x82;
constexpr uint8_t MQTT_UNSUBSCRIBE = 0xA2;
constexpr uint8_t MQTT_PINGREQ = 0xC0;

constexpr uint8_t MQTT_TYPE_CONNACK = 2;
constexpr uint8_t MQTT_TYPE_PUBLISH = 3;

constexpr uint32_t RECONNECT_INTERVAL_MS = 10000;
constexpr uint32_t CONNACK_TIMEOUT_MS = 10000;
constexpr uint32_t MIN_KEEP_ALIVE_SEC = 10;
constexpr uint32_t MAX_KEEP_ALIVE_SEC = 300;
constexpr int MAX_RX_BYTES_PER_ITERATE = 1024;
constexpr uint32_t MIN_TLS_FREE_HEAP = 16000;
constexpr uint32_t MIN_TLS_FREE_BLOCK = 6000;

const char kConnected[] = "MQTT: connected";
const char kConnectionFailed[] = "MQTT: failed to establish connection";
const char kConnectionLost[] = "MQTT: connection lost";
}  // namespace

Supla::Protocol::ArduinoMqtt::ArduinoMqtt(SuplaDeviceClass *sdc)
    : Supla::Protocol::Mqtt(sdc) {
}

Supla::Protocol::ArduinoMqtt::~ArduinoMqtt() {
  closeConnection();
  delete client;
  client = nullptr;
  delete[] mqttCaCert;
  mqttCaCert = nullptr;
}

void Supla::Protocol::ArduinoMqtt::onInit() {
  if (!isEnabled()) {
    return;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  // ESP8266 runs only one communication protocol at a time
  if (cfg != nullptr && cfg->isSuplaCommProtocolEnabled()) {
    SUPLA_LOG_WARNING("MQTT: disabled, SUPLA protocol is enabled");
    sdc->addLastStateLog("MQTT: disabled, SUPLA protocol is enabled");
    enabled = false;
    return;
  }

  Supla::Protocol::Mqtt::onInit();

  delete[] mqttCaCert;
  mqttCaCert = nullptr;
  if (useTls && (cfg == nullptr || cfg->isMqttBrokerVerificationEnabled())) {
    int mqttCaSize = cfg == nullptr ? 0 : cfg->getMqttCASize();
    if (mqttCaSize > 0) {
      mqttCaCert = new char[mqttCaSize + 1]();
      if (mqttCaCert != nullptr &&
          (!cfg->getMqttCA(mqttCaCert, mqttCaSize + 1) ||
           mqttCaCert[0] == '\0')) {
        delete[] mqttCaCert;
        mqttCaCert = nullptr;
      }
    }
    if (mqttCaCert == nullptr) {
      // there is no certificate bundle on ESP8266
      SUPLA_LOG_ERROR("MQTT: broker CA certificate is required");
      sdc->addLastStateLog("MQTT: broker CA certificate is required");
      enabled = false;
      error = true;
      connectionError = ConnectionError::CERTIFICATE_ERROR;
      return;
    }
  }

  uint32_t keepAliveSec = sdc->getActivityTimeout();
  if (keepAliveSec < MIN_KEEP_ALIVE_SEC) {
    keepAliveSec = MIN_KEEP_ALIVE_SEC;
  } else if (keepAliveSec > MAX_KEEP_ALIVE_SEC) {
    keepAliveSec = MAX_KEEP_ALIVE_SEC;
  }
  keepAliveMs = keepAliveSec * 1000;
}

void Supla::Protocol::ArduinoMqtt::disconnect() {
  if (!isEnabled()) {
    return;
  }
  closeConnection();
  connecting = false;
  firstConnectAttempt = true;
}

bool Supla::Protocol::ArduinoMqtt::iterate(uint32_t _millis) {
  if (!isEnabled()) {
    return false;
  }

  uptime.iterate(_millis);

  if (state != State::Idle && !client->connected()) {
    bool wasConnected = state == State::Connected;
    closeConnection();
    setConnectionError(
        wasConnected ? ConnectionError::CONNECTION_LOST
                     : ConnectionError::UNKNOWN,
        wasConnected ? kConnectionLost : kConnectionFailed);
  }

  if (state == State::Idle) {
    if (!firstConnectAttempt &&
        _millis - lastConnectAttemptMs < RECONNECT_INTERVAL_MS) {
      return false;
    }
    firstConnectAttempt = false;
    lastConnectAttemptMs = _millis;
    connectToBroker(_millis);
    return false;
  }

  readIncoming(_millis);

  if (state == State::WaitConnAck) {
    if (_millis - connectStartMs > CONNACK_TIMEOUT_MS) {
      closeConnection();
      setConnectionError(ConnectionError::CONNECTION_TIMEOUT,
                         kConnectionFailed);
    }
    return false;
  }

  if (state != State::Connected) {
    return false;
  }

  if (_millis - lastRxMs > keepAliveMs) {
    closeConnection();
    setConnectionError(ConnectionError::CONNECTION_LOST, kConnectionLost);
    return false;
  }

  if (_millis - lastTxMs >= keepAliveMs / 2) {
    lastTxMs = _millis;
    if (!writeFixedHeader(MQTT_PINGREQ, 0)) {
      closeConnection();
      setConnectionError(ConnectionError::CONNECTION_LOST, kConnectionLost);
      return false;
    }
  }

  if (enterRegisteredAndReady) {
    enterRegisteredAndReady = false;
    publishDeviceStatus(true);
    lastStatusUpdateSec = uptime.getConnectionUptime();

    for (int i = 0; i < channelsCount; i++) {
      publishChannelSetup(i);
    }
  }

  processConfigChanges();

  // send device status update
  if (uptime.getConnectionUptime() - lastStatusUpdateSec >= 5) {
    lastStatusUpdateSec = uptime.getConnectionUptime();
    publishDeviceStatus(false);
  }

  // send channel state updates are done in iterateConnected
  return connected;
}

Supla::ConnectionError
Supla::Protocol::ArduinoMqtt::getConnectionError() const {
  return connectionError;
}

bool Supla::Protocol::ArduinoMqtt::connectToBroker(uint32_t _millis) {
  connecting = true;
  connected = false;

  // BearSSL throws on allocation failure, which would restart the device
  if (useTls && (ESP.getFreeHeap() < MIN_TLS_FREE_HEAP ||
                 ESP.getMaxFreeBlockSize() < MIN_TLS_FREE_BLOCK)) {
    setConnectionError(ConnectionError::TLS_ERROR,
                       "MQTT: not enough memory for TLS");
    return false;
  }

  if (client == nullptr) {
    client = Supla::ClientBuilder();
    client->setSdc(sdc);
    client->setSSLEnabled(useTls);
    client->setCACert(mqttCaCert);
  }

  if (client->connect(server, port) != 1) {
    auto clientError = client->getConnectionError();
    closeConnection();
    setConnectionError(clientError == ConnectionError::NONE
                           ? ConnectionError::UNKNOWN
                           : clientError,
                       kConnectionFailed);
    return false;
  }

  char clientId[MQTT_CLIENTID_MAX_SIZE] = {};
  generateClientId(clientId);
  MqttTopic lastWill(prefix);
  lastWill = lastWill / "state" / "connected";
  const char willPayload[] = "false";

  uint16_t clientIdLen = strlen(clientId);
  uint16_t willTopicLen = strlen(lastWill.c_str());
  uint16_t willPayloadLen = sizeof(willPayload) - 1;
  uint16_t userLen = useAuth ? strlen(user) : 0;
  uint16_t passwordLen = useAuth ? strlen(password) : 0;

  // clean session, last will with retain (QoS 0)
  uint8_t flags = 0x02 | 0x04 | 0x20;
  if (useAuth) {
    flags |= 0x80 | 0x40;
  }

  uint32_t remainingLength = 10 + 2 + clientIdLen + 2 + willTopicLen + 2 +
                             willPayloadLen;
  if (useAuth) {
    remainingLength += 2 + userLen + 2 + passwordLen;
  }

  uint16_t keepAliveSec = keepAliveMs / 1000;
  const uint8_t variableHeader[] = {0x00,
                                    0x04,
                                    'M',
                                    'Q',
                                    'T',
                                    'T',
                                    0x04,
                                    flags,
                                    static_cast<uint8_t>(keepAliveSec >> 8),
                                    static_cast<uint8_t>(keepAliveSec & 0xFF)};

  bool ok = writeFixedHeader(MQTT_CONNECT, remainingLength) &&
            client->write(variableHeader, sizeof(variableHeader)) ==
                sizeof(variableHeader) &&
            writeString(clientId, clientIdLen) &&
            writeString(lastWill.c_str(), willTopicLen) &&
            writeString(willPayload, willPayloadLen);
  if (ok && useAuth) {
    ok = writeString(user, userLen) && writeString(password, passwordLen);
  }

  if (!ok) {
    closeConnection();
    setConnectionError(ConnectionError::CONNECTION_LOST, kConnectionFailed);
    return false;
  }

  state = State::WaitConnAck;
  connectStartMs = _millis;
  lastRxMs = _millis;
  lastTxMs = _millis;
  return true;
}

void Supla::Protocol::ArduinoMqtt::closeConnection() {
  if (client != nullptr) {
    client->stop();
    // TLS client keeps its BearSSL buffers until it is deleted
    if (useTls) {
      delete client;
      client = nullptr;
    }
  }
  state = State::Idle;
  connected = false;
  enterRegisteredAndReady = false;
  rxHeader = 0;
  rxLengthBytes = 0;
  rxRemaining = 0;
  rxLength = 0;
  rxPos = 0;
  rxInBody = false;
}

void Supla::Protocol::ArduinoMqtt::setConnectionError(
    ConnectionError newError, const char *message) {
  error = true;
  connecting = false;
  connected = false;
  enterRegisteredAndReady = false;
  connectionError = newError;
  if (message != lastMessage) {
    lastMessage = message;
    SUPLA_LOG_WARNING("%s", message);
    sdc->addLastStateLog(message);
  }
}

bool Supla::Protocol::ArduinoMqtt::writeFixedHeader(uint8_t type,
                                                    uint32_t remainingLength) {
  uint8_t header[5] = {type};
  size_t size = 1;
  do {
    uint8_t encoded = remainingLength % 128;
    remainingLength /= 128;
    if (remainingLength > 0) {
      encoded |= 0x80;
    }
    header[size++] = encoded;
  } while (remainingLength > 0 && size < sizeof(header));
  return client->write(header, size) == size;
}

bool Supla::Protocol::ArduinoMqtt::writeString(const char *str, uint16_t len) {
  if (!writeUInt16(len)) {
    return false;
  }
  return len == 0 ||
         client->write(reinterpret_cast<const uint8_t *>(str), len) == len;
}

bool Supla::Protocol::ArduinoMqtt::writeUInt16(uint16_t value) {
  const uint8_t buf[2] = {static_cast<uint8_t>(value >> 8),
                          static_cast<uint8_t>(value & 0xFF)};
  return client->write(buf, sizeof(buf)) == sizeof(buf);
}

uint16_t Supla::Protocol::ArduinoMqtt::nextPacketId() {
  packetId++;
  if (packetId == 0) {
    packetId = 1;
  }
  return packetId;
}

void Supla::Protocol::ArduinoMqtt::readIncoming(uint32_t _millis) {
  int count = 0;
  while (state != State::Idle && count < MAX_RX_BYTES_PER_ITERATE &&
         client->available() > 0) {
    int data = client->read();
    if (data < 0) {
      break;
    }
    count++;
    lastRxMs = _millis;
    uint8_t byte = static_cast<uint8_t>(data);

    if (!rxInBody) {
      if (rxHeader == 0) {
        rxHeader = byte;
        rxLength = 0;
        rxLengthBytes = 0;
        continue;
      }
      rxLength |= static_cast<uint32_t>(byte & 0x7F) << (7 * rxLengthBytes);
      rxLengthBytes++;
      if (byte & 0x80) {
        if (rxLengthBytes >= 4) {
          closeConnection();
          setConnectionError(ConnectionError::PROTOCOL_ERROR,
                             "MQTT: protocol error");
        }
        continue;
      }
      rxRemaining = rxLength;
      rxPos = 0;
      rxInBody = true;
    } else {
      if (rxPos < RX_BUFFER_SIZE) {
        rxBuffer[rxPos++] = static_cast<char>(byte);
      }
      rxRemaining--;
    }

    if (rxInBody && rxRemaining == 0) {
      handlePacket();
      rxHeader = 0;
      rxInBody = false;
    }
  }
}

void Supla::Protocol::ArduinoMqtt::handlePacket() {
  uint8_t type = rxHeader >> 4;
  if (type == MQTT_TYPE_CONNACK) {
    if (state != State::WaitConnAck) {
      return;
    }
    uint8_t returnCode =
        rxLength >= 2 ? static_cast<uint8_t>(rxBuffer[1]) : 0xFF;
    if (returnCode == 0) {
      state = State::Connected;
      connecting = false;
      connected = true;
      error = false;
      connectionError = ConnectionError::NONE;
      enterRegisteredAndReady = true;
      uptime.resetConnectionUptime();
      lastStatusUpdateSec = 0;
      memset(configChangedBit, 0, sizeof(configChangedBit));
      if (lastMessage != kConnected) {
        lastMessage = kConnected;
        SUPLA_LOG_INFO("%s", kConnected);
        sdc->addLastStateLog(kConnected);
      }
      return;
    }
    closeConnection();
    switch (returnCode) {
      case 1:
        setConnectionError(ConnectionError::PROTOCOL_ERROR,
                           "MQTT: connection refused (bad protocol)");
        break;
      case 3:
        setConnectionError(ConnectionError::SERVER_UNAVAILABLE,
                           "MQTT: connection refused (server unavailable)");
        break;
      case 4:
        setConnectionError(
            ConnectionError::BAD_CREDENTIALS,
            "MQTT: connection refused (bad username or password)");
        break;
      case 5:
        setConnectionError(ConnectionError::NOT_AUTHORIZED,
                           "MQTT: connection refused (not authorized)");
        break;
      default:
        setConnectionError(ConnectionError::CONNECTION_REFUSED,
                           "MQTT: connection refused");
        break;
    }
    return;
  }

  if (type == MQTT_TYPE_PUBLISH && state == State::Connected) {
    handlePublish();
  }
  // PUBACK, SUBACK, UNSUBACK and PINGRESP need no handling
}

void Supla::Protocol::ArduinoMqtt::handlePublish() {
  uint8_t qos = (rxHeader >> 1) & 0x03;
  uint32_t available = rxPos;
  if (available < 2) {
    return;
  }
  uint16_t topicLen = (static_cast<uint8_t>(rxBuffer[0]) << 8) |
                      static_cast<uint8_t>(rxBuffer[1]);
  uint32_t pos = 2 + topicLen;
  uint16_t receivedId = 0;
  if (qos > 0) {
    if (pos + 2 > available) {
      SUPLA_LOG_WARNING("MQTT: ignoring oversized message");
      return;
    }
    receivedId = (static_cast<uint8_t>(rxBuffer[pos]) << 8) |
                 static_cast<uint8_t>(rxBuffer[pos + 1]);
    pos += 2;
  }

  if (pos > rxLength || topicLen >= MAX_TOPIC_LEN ||
      rxLength - pos >= MQTT_MAX_PAYLOAD_LEN || rxLength > RX_BUFFER_SIZE) {
    SUPLA_LOG_WARNING("MQTT: ignoring invalid or oversized message");
  } else {
    uint32_t payloadLen = rxLength - pos;
    memmove(rxBuffer, rxBuffer + 2, topicLen);
    rxBuffer[topicLen] = '\0';
    memmove(rxBuffer + topicLen + 1, rxBuffer + pos, payloadLen);
    rxBuffer[topicLen + 1 + payloadLen] = '\0';
    processData(rxBuffer, rxBuffer + topicLen + 1);
  }

  if (qos == 1 && state == State::Connected) {
    if (!writeFixedHeader(MQTT_PUBACK, 2) || !writeUInt16(receivedId)) {
      closeConnection();
      setConnectionError(ConnectionError::CONNECTION_LOST, kConnectionLost);
    }
  }
}

void Supla::Protocol::ArduinoMqtt::publishImp(const char *topic,
                                              const char *payload,
                                              int qos,
                                              bool retain) {
  if (state != State::Connected) {
    return;
  }
  // QoS 2 is not supported, use QoS 1 instead
  if (qos > 1) {
    qos = 1;
  } else if (qos < 0) {
    qos = 0;
  }

  uint16_t topicLen = strlen(topic);
  size_t payloadLen = strlen(payload);
  uint32_t remainingLength = 2 + topicLen + (qos > 0 ? 2 : 0) + payloadLen;
  uint8_t type = MQTT_PUBLISH | (qos << 1) | (retain ? 0x01 : 0x00);

  bool ok = writeFixedHeader(type, remainingLength) &&
            writeString(topic, topicLen) &&
            (qos == 0 || writeUInt16(nextPacketId())) &&
            (payloadLen == 0 ||
             client->write(reinterpret_cast<const uint8_t *>(payload),
                           payloadLen) == payloadLen);
  if (!ok) {
    closeConnection();
    setConnectionError(ConnectionError::CONNECTION_LOST, kConnectionLost);
  }
}

void Supla::Protocol::ArduinoMqtt::subscribeImp(const char *topic, int qos) {
  if (state != State::Connected) {
    return;
  }
  // incoming QoS 2 messages are not supported
  uint8_t requestedQos = qos > 0 ? 1 : 0;
  uint16_t topicLen = strlen(topic);
  bool ok = writeFixedHeader(MQTT_SUBSCRIBE, 2 + 2 + topicLen + 1) &&
            writeUInt16(nextPacketId()) && writeString(topic, topicLen) &&
            client->write(&requestedQos, 1) == 1;
  if (!ok) {
    closeConnection();
    setConnectionError(ConnectionError::CONNECTION_LOST, kConnectionLost);
  }
}

void Supla::Protocol::ArduinoMqtt::unsubscribeImp(const char *topic) {
  if (state != State::Connected) {
    return;
  }
  uint16_t topicLen = strlen(topic);
  bool ok = writeFixedHeader(MQTT_UNSUBSCRIBE, 2 + 2 + topicLen) &&
            writeUInt16(nextPacketId()) && writeString(topic, topicLen);
  if (!ok) {
    closeConnection();
    setConnectionError(ConnectionError::CONNECTION_LOST, kConnectionLost);
  }
}

#endif  // ARDUINO_ARCH_ESP8266
