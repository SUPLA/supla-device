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

#include "general_purpose_text_parsed.h"

#include <supla/log_wrapper.h>

#include <string>
#include <variant>

namespace {
constexpr char kValueParameter[] = "value";
}

using Supla::Sensor::GeneralPurposeTextParsed;

GeneralPurposeTextParsed::GeneralPurposeTextParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void GeneralPurposeTextParsed::iterateAlways() {
  if (isParameterConfigured(kValueParameter)) {
    if (refreshParserSource()) {
      const auto value = getStateParameterValue(kValueParameter);
      if (parser->isValid()) {
        std::string text;
        if (const auto *stringValue = std::get_if<std::string>(&value)) {
          text = *stringValue;
        } else if (const auto *boolValue = std::get_if<bool>(&value)) {
          text = *boolValue ? "true" : "false";
        } else {
          text = std::to_string(std::get<int>(value));
        }
        setValue(text.c_str());
        isDataErrorLogged = false;
      }
    }
    if (!parser->isValid() && !isDataErrorLogged) {
      isDataErrorLogged = true;
      SUPLA_LOG_WARNING("GPT: source data error");
    }
  }

  GeneralPurposeText::iterateAlways();
}