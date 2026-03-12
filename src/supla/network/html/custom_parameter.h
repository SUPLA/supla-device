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

#ifndef SRC_SUPLA_NETWORK_HTML_CUSTOM_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_CUSTOM_PARAMETER_H_

#ifndef ARDUINO_ARCH_AVR

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <supla/network/html_element.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

#include <limits>
#include <type_traits>

namespace Supla {

namespace Html {

/**
 * This HTML Element provides numeric input in config mode.
 * Value is always stored in Supla::Storage::Config as int32_t.
 * For floating point types decimalPlaces defines storage scale and HTML step.
 */
template <typename T>
class CustomParameterTemplate : public HtmlElement {
 public:
  CustomParameterTemplate(const char* paramTag,
                          const char* paramLabel,
                          T defaultValue = static_cast<T>(0),
                          T minValue = std::numeric_limits<T>::lowest(),
                          T maxValue = std::numeric_limits<T>::max(),
                          uint8_t decimalPlaces = 0);

  virtual ~CustomParameterTemplate();

  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

  T getParameterValue();
  void setParameterValue(T newValue);

 protected:
  static_assert(
      (std::is_integral<T>::value && std::is_signed<T>::value) ||
          std::is_floating_point<T>::value,
      "CustomParameterTemplate supports signed integers and floating types");

  int32_t parameterValue = 0;
  char* tag = nullptr;
  char* label = nullptr;
  int32_t minStoredValue = 0;
  int32_t maxStoredValue = 0;
  uint8_t decimalPlaces = 0;

  static uint8_t sanitizeDecimalPlaces(uint8_t requestedDecimalPlaces);
  static int32_t getScaleFactor(uint8_t decimalPlaces);
  static int32_t clampToInt32(long double value);

  int32_t encodeValue(T value) const;
  T decodeValue(int32_t value) const;
  void formatValue(char* buffer, size_t bufferSize, int32_t value) const;
  void sendStepAttribute(Supla::WebSender* sender) const;
  void setStoredValue(int32_t newValue, bool saveToConfig);
};

template <typename T>
CustomParameterTemplate<T>::CustomParameterTemplate(const char* paramTag,
                                                    const char* paramLabel,
                                                    T defaultValue,
                                                    T minValue,
                                                    T maxValue,
                                                    uint8_t decimalPlaces)
: HtmlElement(HTML_SECTION_FORM),
decimalPlaces(sanitizeDecimalPlaces(decimalPlaces)) {
  if (paramTag != nullptr) {
    int size = strlen(paramTag);
    if (size < 500) {
      tag = new char[size + 1];
      strncpy(tag, paramTag, size + 1);
    }
  }

  if (paramLabel != nullptr) {
    int size = strlen(paramLabel);
    if (size < 500) {
      label = new char[size + 1];
      strncpy(label, paramLabel, size + 1);
    }
  }

  minStoredValue = encodeValue(minValue);
  maxStoredValue = encodeValue(maxValue);
  if (minStoredValue > maxStoredValue) {
    const int32_t tmp = minStoredValue;
    minStoredValue = maxStoredValue;
    maxStoredValue = tmp;
  }

  setStoredValue(encodeValue(defaultValue), false);
}

template <typename T>
CustomParameterTemplate<T>::~CustomParameterTemplate() {
  if (tag != nullptr) {
    delete[] tag;
    tag = nullptr;
  }
  if (label != nullptr) {
    delete[] label;
    label = nullptr;
  }
}

template <typename T>
uint8_t CustomParameterTemplate<T>::sanitizeDecimalPlaces(
    uint8_t requestedDecimalPlaces) {
  if (std::is_integral<T>::value) {
    return 0;
  }
  return requestedDecimalPlaces > 6 ? 6 : requestedDecimalPlaces;
}

template <typename T>
int32_t CustomParameterTemplate<T>::getScaleFactor(uint8_t decimalPlaces) {
  int32_t factor = 1;
  for (uint8_t i = 0; i < decimalPlaces; i++) {
    factor *= 10;
  }
  return factor;
}

template <typename T>
int32_t CustomParameterTemplate<T>::clampToInt32(long double value) {
  if (value > static_cast<long double>(INT32_MAX)) {
    return INT32_MAX;
  }
  if (value < static_cast<long double>(INT32_MIN)) {
    return INT32_MIN;
  }
  return static_cast<int32_t>(value);
}

template <typename T>
int32_t CustomParameterTemplate<T>::encodeValue(T value) const {
  if (std::is_integral<T>::value) {
    return clampToInt32(static_cast<long double>(value));
  }

  const long double scale =
      static_cast<long double>(getScaleFactor(decimalPlaces));
  long double scaled = static_cast<long double>(value) * scale;
  if (scaled >= 0) {
    scaled += 0.5;
  } else {
    scaled -= 0.5;
  }
  return clampToInt32(scaled);
}

template <typename T>
T CustomParameterTemplate<T>::decodeValue(int32_t value) const {
  if (std::is_integral<T>::value) {
    return static_cast<T>(value);
  }

  return static_cast<T>(static_cast<long double>(value) /
                        getScaleFactor(decimalPlaces));
}

template <typename T>
void CustomParameterTemplate<T>::formatValue(char* buffer,
                                             size_t bufferSize,
                                             int32_t value) const {
  if (std::is_integral<T>::value) {
    snprintf(buffer, bufferSize, "%" PRId32, static_cast<int32_t>(value));
    return;
  }

  snprintf(buffer,
           bufferSize,
           "%.*f",
           decimalPlaces,
           static_cast<double>(decodeValue(value)));
}

template <typename T>
void CustomParameterTemplate<T>::sendStepAttribute(
    Supla::WebSender* sender) const {
  sender->send(" step=\"");
  if (std::is_integral<T>::value || decimalPlaces == 0) {
    sender->send("1");
  } else {
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "0.%0*d", decimalPlaces, 1);
    sender->send(buf);
  }
  sender->send("\"");
}

template <typename T>
void CustomParameterTemplate<T>::setStoredValue(int32_t newValue,
                                                bool saveToConfig) {
  if (newValue < minStoredValue) {
    newValue = minStoredValue;
  }
  if (newValue > maxStoredValue) {
    newValue = maxStoredValue;
  }

  parameterValue = newValue;

  auto cfg = Supla::Storage::ConfigInstance();
  if (saveToConfig && cfg) {
    cfg->setInt32(tag, parameterValue);
    cfg->saveWithDelay(1000);
  }
}

template <typename T>
void CustomParameterTemplate<T>::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t storedValue = parameterValue;
    if (cfg->getInt32(tag, &storedValue)) {
      setStoredValue(storedValue, false);
    }
  }

  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(tag, label);
  sender->send("<input type=\"number\"");
  sendStepAttribute(sender);

  char buf[100] = {};
  sender->send(" min=\"");
  formatValue(buf, sizeof(buf), minStoredValue);
  sender->send(buf);
  sender->send("\" max=\"");
  formatValue(buf, sizeof(buf), maxStoredValue);
  sender->send(buf);
  sender->send("\" ");
  sender->sendNameAndId(tag);
  sender->send(" value=\"");
  formatValue(buf, sizeof(buf), parameterValue);
  sender->send(buf);
  sender->send("\">");
  sender->send("</div>");
}

template <typename T>
bool CustomParameterTemplate<T>::handleResponse(const char* key,
                                                const char* value) {
  if (key == nullptr || value == nullptr || tag == nullptr ||
      strcmp(key, tag) != 0) {
    return false;
  }

  int32_t parsedValue = 0;
  if (std::is_integral<T>::value) {
    parsedValue = stringToInt(value);
  } else {
    parsedValue = floatStringToInt(value, decimalPlaces);
  }

  setStoredValue(parsedValue, true);
  return true;
}

template <typename T>
T CustomParameterTemplate<T>::getParameterValue() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t storedValue = parameterValue;
    if (cfg->getInt32(tag, &storedValue)) {
      setStoredValue(storedValue, false);
    }
  }
  return decodeValue(parameterValue);
}

template <typename T>
void CustomParameterTemplate<T>::setParameterValue(T newValue) {
  setStoredValue(encodeValue(newValue), true);
}

using CustomParameter = CustomParameterTemplate<int32_t>;

};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_NETWORK_HTML_CUSTOM_PARAMETER_H_
