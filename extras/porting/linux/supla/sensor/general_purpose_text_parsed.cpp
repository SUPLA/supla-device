// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
