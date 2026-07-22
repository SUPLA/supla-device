/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "mqtt_documentation.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

namespace {

const char *operationName(MqttCapturedOperation::Type type) {
  return type == MqttCapturedOperation::Type::Publish ? "publish" : "subscribe";
}

const char *directionName(MqttCapturedOperation::Type type) {
  return type == MqttCapturedOperation::Type::Publish ? "device_to_broker"
                                                      : "broker_to_device";
}

void createParentDirectories(const std::string &path) {
  const auto lastSlash = path.find_last_of('/');
  if (lastSlash == std::string::npos) {
    return;
  }

  const std::string parent = path.substr(0, lastSlash);
  std::string current;
  size_t position = 0;
  if (!parent.empty() && parent[0] == '/') {
    current = "/";
    position = 1;
  }

  while (position < parent.size()) {
    const auto nextSlash = parent.find('/', position);
    const std::string component = parent.substr(
        position,
        nextSlash == std::string::npos ? std::string::npos
                                       : nextSlash - position);
    if (!component.empty()) {
      if (!current.empty() && current != "/") {
        current += '/';
      }
      current += component;
      if (mkdir(current.c_str(), 0777) != 0 && errno != EEXIST) {
        throw std::runtime_error(
            "cannot create MQTT documentation directory: " + current);
      }
    }
    if (nextSlash == std::string::npos) {
      break;
    }
    position = nextSlash + 1;
  }
}

}  // namespace

MqttDocumentationRecorder::MqttDocumentationRecorder(bool enabled,
                                                     std::string outputPath)
    : enabled_(enabled), outputPath_(std::move(outputPath)) {
}

MqttDocumentationRecorder::~MqttDocumentationRecorder() {
  if (enabled_ && !outputPath_.empty()) {
    try {
      writeJson(outputPath_);
    } catch (const std::exception &error) {
      std::fprintf(
          stderr, "MQTT documentation capture failed: %s\n", error.what());
      std::fflush(stderr);
      std::_Exit(EXIT_FAILURE);
    }
  }
}

MqttDocumentationRecorder &MqttDocumentationRecorder::fromEnvironment() {
  const char *path = std::getenv("MQTT_DOC_CAPTURE");
  static MqttDocumentationRecorder recorder(path != nullptr && path[0] != '\0',
                                            path == nullptr ? "" : path);
  return recorder;
}

bool MqttDocumentationRecorder::isEnabled() const {
  return enabled_;
}

bool MqttDocumentationRecorder::hasActiveScenario() const {
  return activeScenario_.has_value();
}

void MqttDocumentationRecorder::beginScenario(
    const MqttDocumentationScenarioMetadata &metadata) {
  if (!enabled_) {
    return;
  }
  if (activeScenario_.has_value()) {
    throw std::logic_error(
        "nested MQTT documentation scenarios are not allowed");
  }
  if (metadata.id.empty()) {
    throw std::invalid_argument("MQTT documentation scenario id is empty");
  }
  activeScenario_ = metadata;
  auto [it, inserted] =
      scenarios_.try_emplace(metadata.id, MqttCapturedScenario{metadata, {}});
  if (!inserted &&
      (it->second.metadata.description != metadata.description ||
       it->second.metadata.channelType != metadata.channelType ||
       it->second.metadata.channelFunction != metadata.channelFunction ||
       it->second.metadata.category != metadata.category ||
       it->second.metadata.channelNumber != metadata.channelNumber ||
       it->second.metadata.devicePrefix != metadata.devicePrefix)) {
    activeScenario_.reset();
    throw std::logic_error("MQTT documentation scenario metadata conflict: " +
                           metadata.id);
  }
}

void MqttDocumentationRecorder::endScenario() {
  if (enabled_) {
    activeScenario_.reset();
  }
}

void MqttDocumentationRecorder::recordPublish(const char *topic,
                                              const char *payload,
                                              int qos,
                                              bool retain) {
  record({MqttCapturedOperation::Type::Publish,
          topic == nullptr ? "" : topic,
          payload == nullptr ? "" : payload,
          qos,
          retain,
          "",
          {}});
}

void MqttDocumentationRecorder::recordSubscribe(const char *topic, int qos) {
  record({MqttCapturedOperation::Type::Subscribe,
          topic == nullptr ? "" : topic,
          "",
          qos,
          false,
          "",
          {}});
}

void MqttDocumentationRecorder::record(MqttCapturedOperation operation) {
  if (!enabled_ || !activeScenario_.has_value()) {
    return;
  }
  operation.category = activeScenario_->category;
  if (operation.type == MqttCapturedOperation::Type::Publish &&
      operation.payload.empty() && operation.retain) {
    operation.category = "cleanup";
  } else if (operation.category == "mixed") {
    operation.category = "public";
  }
  operation.topic = normalizeTopic(operation.topic, &operation.variables);
  auto &operations = scenarios_.at(activeScenario_->id).operations;
  const auto duplicate = std::find_if(
      operations.begin(), operations.end(), [&](const auto &existing) {
        return std::tie(existing.type,
                        existing.topic,
                        existing.payload,
                        existing.qos,
                        existing.retain,
                        existing.category,
                        existing.variables) == std::tie(operation.type,
                                                        operation.topic,
                                                        operation.payload,
                                                        operation.qos,
                                                        operation.retain,
                                                        operation.category,
                                                        operation.variables);
      });
  if (duplicate == operations.end()) {
    operations.push_back(std::move(operation));
  }
}

std::string MqttDocumentationRecorder::normalizeTopic(
    const std::string &topic, std::map<std::string, int> *variables) const {
  std::string normalized = topic;
  if (!activeScenario_->devicePrefix.empty() &&
      normalized.starts_with(activeScenario_->devicePrefix)) {
    normalized.replace(0, activeScenario_->devicePrefix.size(), "{prefix}");
  }

  if (activeScenario_->channelNumber >= 0) {
    const std::string channel =
        "/channels/" + std::to_string(activeScenario_->channelNumber);
    const auto channelPosition = normalized.find(channel);
    if (channelPosition != std::string::npos) {
      normalized.replace(
          channelPosition, channel.size(), "/channels/{channel}");
    }

    const std::string discoveryChannel =
        "_" + std::to_string(activeScenario_->channelNumber) + "_";
    const auto discoveryChannelPosition = normalized.find(discoveryChannel);
    if (normalized.starts_with("homeassistant/") &&
        discoveryChannelPosition != std::string::npos) {
      normalized.replace(discoveryChannelPosition,
                         discoveryChannel.size(),
                         "_{channel}_");
      const auto subIdPosition = discoveryChannelPosition + 11;
      const auto configPosition = normalized.find("/config", subIdPosition);
      if (configPosition != std::string::npos &&
          subIdPosition < configPosition) {
        if (variables != nullptr) {
          (*variables)["sub_id"] = std::stoi(
              normalized.substr(subIdPosition, configPosition - subIdPosition));
        }
        normalized.replace(subIdPosition,
                           configPosition - subIdPosition,
                           "{sub_id}");
      }
    }
  }

  const std::string phases = "/phases/";
  const auto phasePosition = normalized.find(phases);
  if (phasePosition != std::string::npos) {
    const auto valuePosition = phasePosition + phases.size();
    if (valuePosition < normalized.size() && normalized[valuePosition] >= '1' &&
        normalized[valuePosition] <= '3' &&
        (valuePosition + 1 == normalized.size() ||
         normalized[valuePosition + 1] == '/')) {
      if (variables != nullptr) {
        (*variables)["phase"] = normalized[valuePosition] - '0';
      }
      normalized.replace(valuePosition, 1, "{phase}");
    }
  }
  return normalized;
}

const std::map<std::string, MqttCapturedScenario> &
MqttDocumentationRecorder::scenarios() const {
  return scenarios_;
}

nlohmann::json MqttDocumentationRecorder::toJson() const {
  nlohmann::json result = {{"schema_version", 1},
                           {"scenarios", nlohmann::json::array()}};
  for (const auto &[id, captured] : scenarios_) {
    auto operations = captured.operations;
    std::sort(operations.begin(),
              operations.end(),
              [](const auto &left, const auto &right) {
                return std::tie(left.topic,
                                left.type,
                                left.variables,
                                left.payload,
                                left.qos,
                                left.retain,
                                left.category) < std::tie(right.topic,
                                                          right.type,
                                                          right.variables,
                                                          right.payload,
                                                          right.qos,
                                                          right.retain,
                                                          right.category);
              });

    nlohmann::json scenario = {
        {"id", id},
        {"description", captured.metadata.description},
        {"channel_type", captured.metadata.channelType},
        {"channel_function", captured.metadata.channelFunction},
        {"category", captured.metadata.category},
        {"operations", nlohmann::json::array()},
    };
    for (const auto &operation : operations) {
      nlohmann::json value = {
          {"direction", directionName(operation.type)},
          {"operation", operationName(operation.type)},
          {"topic", operation.topic},
          {"qos", operation.qos},
          {"retain", operation.retain},
      };
      if (operation.type == MqttCapturedOperation::Type::Publish) {
        value["payload_example"] = operation.payload;
      }
      if (operation.category != captured.metadata.category) {
        value["category"] = operation.category;
      }
      if (!operation.variables.empty()) {
        value["variables"] = operation.variables;
      }
      scenario["operations"].push_back(std::move(value));
    }
    result["scenarios"].push_back(std::move(scenario));
  }
  return result;
}

void MqttDocumentationRecorder::writeJson(const std::string &path) const {
  createParentDirectories(path);
  std::ofstream stream(path);
  if (!stream) {
    throw std::runtime_error("cannot write MQTT documentation capture: " +
                             path);
  }
  stream << toJson().dump(2) << '\n';
  if (!stream) {
    throw std::runtime_error("failed to write MQTT documentation capture: " +
                             path);
  }
}

MqttDocumentationScenario::MqttDocumentationScenario(
    MqttDocumentationRecorder &recorder,
    MqttDocumentationScenarioMetadata metadata)
    : recorder_(recorder), active_(recorder.isEnabled()) {
  recorder_.beginScenario(metadata);
}

MqttDocumentationScenario::~MqttDocumentationScenario() {
  if (active_) {
    recorder_.endScenario();
  }
}
