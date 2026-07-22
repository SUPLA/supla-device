/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef EXTRAS_TEST_DOUBLES_MQTT_DOCUMENTATION_H_
#define EXTRAS_TEST_DOUBLES_MQTT_DOCUMENTATION_H_

#include <map>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <vector>

struct MqttCapturedOperation {
  enum class Type { Publish, Subscribe };

  Type type;
  std::string topic;
  std::string payload;
  int qos;
  bool retain;
  std::string category;
  std::map<std::string, int> variables;
};

struct MqttDocumentationScenarioMetadata {
  std::string id;
  std::string description;
  std::string channelType;
  std::string channelFunction;
  std::string category;
  int channelNumber = -1;
  std::string devicePrefix;
};

struct MqttCapturedScenario {
  MqttDocumentationScenarioMetadata metadata;
  std::vector<MqttCapturedOperation> operations;
};

class MqttDocumentationRecorder {
 public:
  explicit MqttDocumentationRecorder(bool enabled = true,
                                     std::string outputPath = {});
  ~MqttDocumentationRecorder();

  MqttDocumentationRecorder(const MqttDocumentationRecorder &) = delete;
  MqttDocumentationRecorder &operator=(const MqttDocumentationRecorder &) =
      delete;

  static MqttDocumentationRecorder &fromEnvironment();

  bool isEnabled() const;
  bool hasActiveScenario() const;
  void beginScenario(const MqttDocumentationScenarioMetadata &metadata);
  void endScenario();
  void recordPublish(const char *topic,
                     const char *payload,
                     int qos,
                     bool retain);
  void recordSubscribe(const char *topic, int qos);

  const std::map<std::string, MqttCapturedScenario> &scenarios() const;
  nlohmann::json toJson() const;
  void writeJson(const std::string &path) const;

 private:
  void record(MqttCapturedOperation operation);
  std::string normalizeTopic(const std::string &topic,
                             std::map<std::string, int> *variables) const;

  bool enabled_;
  std::string outputPath_;
  std::optional<MqttDocumentationScenarioMetadata> activeScenario_;
  std::map<std::string, MqttCapturedScenario> scenarios_;
};

class MqttDocumentationScenario {
 public:
  MqttDocumentationScenario(MqttDocumentationRecorder &recorder,
                            MqttDocumentationScenarioMetadata metadata);
  ~MqttDocumentationScenario();

  MqttDocumentationScenario(const MqttDocumentationScenario &) = delete;
  MqttDocumentationScenario &operator=(const MqttDocumentationScenario &) =
      delete;

 private:
  MqttDocumentationRecorder &recorder_;
  bool active_;
};

#define MQTT_DOC_SCENARIO(                                                \
    recorder, id, description, type, function, channel, category, prefix) \
  MqttDocumentationScenario mqttDocumentationScenario(                    \
      recorder,                                                           \
      {id, description, #type, #function, category, channel, prefix})

#endif  // EXTRAS_TEST_DOUBLES_MQTT_DOCUMENTATION_H_
