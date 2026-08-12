// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "json.h"

#include <supla/log_wrapper.h>

#include <cmath>
#include <stdexcept>
#include <string>

Supla::Parser::Json::Json(Supla::Source::Source* src)
    : Supla::Parser::Parser(src) {
}

Supla::Parser::Json::~Json() {
}

bool Supla::Parser::Json::refreshSource() {
  valid = false;
  sourceValid = false;
  if (source) {
    std::string sourceContent = source->getContent();

    if (sourceContent.length() == 0) {
      return valid;
    }

    try {
      json = nlohmann::json::parse(sourceContent);
    } catch (nlohmann::json::parse_error& ex) {
      SUPLA_LOG_ERROR("JSON parsing error at byte %d", ex.byte);
      SUPLA_LOG_ERROR("JSON Source: \n%s", sourceContent.c_str());
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
    const nlohmann::json* valuePtr = nullptr;

    if (!key.empty() && key[0] == '/') {
      nlohmann::json::json_pointer ptr(key);

      if (!json.contains(ptr)) {
        SUPLA_LOG_ERROR("JSON key \"%s\" not found", key.c_str());
        valid = false;
        return 0;
      }

      valuePtr = &json.at(ptr);
    } else {
      if (!json.contains(key)) {
        SUPLA_LOG_ERROR("JSON key \"%s\" not found", key.c_str());
        valid = false;
        return 0;
      }

      valuePtr = &json.at(key);
    }

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
    SUPLA_LOG_ERROR("JSON exception during getValue for key \"%s\": %s",
                    key.c_str(), ex.what());
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
    if (key[0] == '/') {
      return json[nlohmann::json::json_pointer(key)].get<bool>();
    } else {
      return json[key].get<bool>();
    }
  } catch (nlohmann::json::type_error& ex) {
    try {
      if (key[0] == '/') {
        return json[nlohmann::json::json_pointer(key)].get<int>();
      } else {
        return json[key].get<int>();
      }
    } catch (nlohmann::json::type_error& ex) {
      try {
        if (key[0] == '/') {
          return json[nlohmann::json::json_pointer(key)].get<std::string>();
        } else {
          return json[key].get<std::string>();
        }
      } catch (nlohmann::json::type_error& ex) {
        valid = false;
      } catch (...) {
        SUPLA_LOG_ERROR("JSON exception during getStateValue for key \"%s\"",
                        key.c_str());
        valid = false;
      }
    } catch (...) {
      SUPLA_LOG_ERROR("JSON exception during getStateValue for key \"%s\"",
                      key.c_str());
      valid = false;
    }
  } catch (...) {
    SUPLA_LOG_ERROR("JSON exception during getStateValue for key \"%s\"",
                    key.c_str());
    valid = false;
  }
  return 0;
}

bool Supla::Parser::Json::isBasedOnIndex() {
  return false;
}
