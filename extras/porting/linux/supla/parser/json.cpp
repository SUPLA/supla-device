/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include <supla/log_wrapper.h>

#include <stdexcept>

#include "json.h"

Supla::Parser::Json::Json(Supla::Source::Source* src)
    : Supla::Parser::Parser(src) {
}

Supla::Parser::Json::~Json() {
}

bool Supla::Parser::Json::refreshSource() {
  valid = false;
  if (source) {
    std::string sourceContent = source->getContent();

    if (sourceContent.length() == 0) {
      return valid;
    }

    SUPLA_LOG_VERBOSE("Source: %s", sourceContent.c_str());
    try {
      json = nlohmann::json::parse(sourceContent);
    } catch (nlohmann::json::parse_error& ex) {
      SUPLA_LOG_ERROR("JSON parsing error at byte %d", ex.byte);
      return valid;
    }

    valid = true;
  }
  return valid;
}

bool Supla::Parser::Json::isValid() {
  return valid;
}

double Supla::Parser::Json::getValue(const std::string& key) {
  try {
    if (key[0] == '/') {
      return json[nlohmann::json::json_pointer(key)].get<double>();
    } else {
      return json[key].get<double>();
    }
  }
  catch (nlohmann::json::type_error& ex) {
    SUPLA_LOG_ERROR("JSON key \"%s\" not found", key.c_str());
    valid = false;
  }
  catch (...) {
    SUPLA_LOG_ERROR(
        "JSON exception during getValue for key \"%s\"", key.c_str());
    valid = false;
  }

  return 0;
}

std::variant<int, bool, std::string> Supla::Parser::Json::getStateValue(
    const std::string& key) {
  try {
    if (key[0] == '/') {
      return json[nlohmann::json::json_pointer(key)].get<bool>();
    } else {
      return json[key].get<bool>();
    }
  }
  catch (nlohmann::json::type_error& ex) {
    try {
      if (key[0] == '/') {
        return json[nlohmann::json::json_pointer(key)].get<int>();
      } else {
        return json[key].get<int>();
      }
    }
    catch (nlohmann::json::type_error& ex) {
      try {
        if (key[0] == '/') {
          return json[nlohmann::json::json_pointer(key)].get<std::string>();
        } else {
          return json[key].get<std::string>();
        }
      }
      catch (nlohmann::json::type_error& ex) {
        valid = false;
      }
      catch (...) {
        SUPLA_LOG_ERROR(
            "JSON exception during getStateValue for key \"%s\"", key.c_str());
        valid = false;
      }
    }
    catch (...) {
      SUPLA_LOG_ERROR(
          "JSON exception during getStateValue for key \"%s\"", key.c_str());
      valid = false;
    }
  }
  catch (...) {
    SUPLA_LOG_ERROR(
        "JSON exception during getStateValue for key \"%s\"", key.c_str());
    valid = false;
  }
  return 0;
}

bool Supla::Parser::Json::isBasedOnIndex() {
  return false;
}
