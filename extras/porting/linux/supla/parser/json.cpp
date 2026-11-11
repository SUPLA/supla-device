// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "json.h"

#include <supla/log_wrapper.h>

#include <cmath>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

Supla::Parser::Json::Json(Supla::Source::Source* src, bool multiDocument)
    : Supla::Parser::Parser(src), multiDocument(multiDocument) {
}

Supla::Parser::Json::~Json() {
}

void Supla::Parser::Json::addKey(const std::string& key, int index) {
  if (multiDocument) {
    const auto separator = key.find(":/");
    if (separator == std::string::npos || separator == 0) {
      throw std::invalid_argument("Expected document-id:/json-pointer");
    }
    try {
      nlohmann::json::json_pointer pointer(key.substr(separator + 1));
    } catch (...) {
      throw std::invalid_argument("Invalid document JSON Pointer");
    }
    if (!source) {
      throw std::invalid_argument("JSON mapping requires a source");
    }
    const auto ids = source->getDocumentIds();
    if (std::find(ids.begin(), ids.end(), key.substr(0, separator)) ==
        ids.end()) {
      throw std::invalid_argument("Unknown document id in JSON mapping");
    }
  }
  Parser::addKey(key, index);
}

const nlohmann::json& Supla::Parser::Json::valueAt(
    const std::string& key) const {
  if (multiDocument) {
    const auto separator = key.find(":/");
    if (separator == std::string::npos || separator == 0) {
      throw std::invalid_argument("Invalid document mapping");
    }
    return documents.at(key.substr(0, separator)).at(
        nlohmann::json::json_pointer(key.substr(separator + 1)));
  }
  if (!key.empty() && key[0] == '/') {
    return json.at(nlohmann::json::json_pointer(key));
  }
  return json.at(key);
}

bool Supla::Parser::Json::refreshSource() {
  valid = false;
  sourceValid = false;
  if (source && multiDocument) {
    documents.clear();
    std::set<std::string> needed;
    for (const auto& key : keys) {
      needed.insert(key.first.substr(0, key.first.find(":/")));
    }
    for (const auto& document : source->getDocuments()) {
      if (!needed.count(document.id)) {
        continue;
      }
      if (!document.received || !document.fresh) {
        return false;
      }
      auto parsed = nlohmann::json::parse(document.content, nullptr, false);
      if (parsed.is_discarded()) {
        SUPLA_LOG_ERROR("JSON document parsing error");
        return false;
      }
      documents.emplace(document.id, std::move(parsed));
    }
    if (documents.size() != needed.size()) {
      return false;
    }
    sourceValid = true;
    valid = true;
    return true;
  }
  if (source) {
    std::string sourceContent = source->getContent();

    if (sourceContent.length() == 0) {
      return valid;
    }

    try {
      json = nlohmann::json::parse(sourceContent);
    } catch (nlohmann::json::parse_error& ex) {
      SUPLA_LOG_ERROR("JSON parsing error at byte %d", ex.byte);
      return valid;
    }

    sourceValid = true;
    valid = true;
  }
  return valid;
}

bool Supla::Parser::Json::isValid() {
  return valid;
}

bool Supla::Parser::Json::isSourceValid() {
  return sourceValid;
}

double Supla::Parser::Json::getValue(const std::string& key) {
  if (!sourceValid) {
    valid = false;
    return 0;
  }

  valid = true;
  try {
    const auto* valuePtr = &valueAt(key);

    if (valuePtr->is_number()) {
      double value = valuePtr->get<double>();
      if (!std::isfinite(value)) {
        SUPLA_LOG_ERROR("JSON key \"%s\" has non-finite numeric value",
                        key.c_str());
        valid = false;
        return 0;
      }
      return value;
    }

    if (valuePtr->is_string()) {
      // Try to convert string to double
      try {
        double value = std::stod(valuePtr->get<std::string>());
        if (!std::isfinite(value)) {
          SUPLA_LOG_ERROR("JSON key \"%s\" has non-finite numeric value",
                          key.c_str());
          valid = false;
          return 0;
        }
        return value;
      } catch (...) {
        SUPLA_LOG_ERROR("JSON key \"%s\" string cannot be converted to double",
                        key.c_str());
        valid = false;
        return 0;
      }
    }

    SUPLA_LOG_ERROR("JSON key \"%s\" has invalid data type (double expected)",
                    key.c_str());
    valid = false;
    return 0;
  } catch (const nlohmann::json::out_of_range&) {
    SUPLA_LOG_ERROR("JSON key \"%s\" not found", key.c_str());
    valid = false;
  } catch (const nlohmann::json::type_error&) {
    SUPLA_LOG_ERROR("JSON key \"%s\" has invalid data type (double expected)",
                    key.c_str());
    valid = false;
  } catch (const std::exception& ex) {
    SUPLA_LOG_ERROR("JSON value lookup failed");
    valid = false;
  }

  return 0;
}

std::variant<int, bool, std::string> Supla::Parser::Json::getStateValue(
    const std::string& key) {
  if (!sourceValid) {
    valid = false;
    return 0;
  }

  valid = true;
  try {
    const auto& value = valueAt(key);
    if (value.is_boolean()) {
      return value.get<bool>();
    }
    if (value.is_number()) {
      return value.get<int>();
    }
    if (value.is_string()) {
      return value.get<std::string>();
    }
  } catch (...) {
    SUPLA_LOG_ERROR("JSON state lookup failed");
  }
  valid = false;
  return 0;
}

bool Supla::Parser::Json::isBasedOnIndex() {
  return false;
}
