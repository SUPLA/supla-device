// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tools.h"
#include <string.h>
#include <supla-common/proto.h>
#include <ctype.h>
#include <limits.h>

#if defined(ARDUINO_ARCH_AVR)
#include <stdlib.h>
#else
#include <cstdlib>
#endif

#if defined(ESP8266)
#include <Esp.h>
#elif defined(ESP32) || defined(SUPLA_DEVICE_ESP32)
#include <esp_random.h>
#endif

#include <supla/log_wrapper.h>
#include "supla/IEEE754tools.h"

void float2DoublePacked(float number, uint8_t *bar, int byteOrder) {
  (void)(byteOrder);
  _FLOATCONV fl = {};
  fl.f = number;
  _DBLCONV dbl;
  dbl.p.s = fl.p.s;
  dbl.p.e = fl.p.e - 127 + 1023;  // exponent adjust
  dbl.p.m = fl.p.m;

#ifdef IEEE754_ENABLE_MSB
  if (byteOrder == LSBFIRST) {
#endif
    for (int i = 0; i < 8; i++) {
      bar[i] = dbl.b[i];
    }
#ifdef IEEE754_ENABLE_MSB
  } else {
    for (int i = 0; i < 8; i++) {
      bar[i] = dbl.b[7 - i];
    }
  }
#endif
}

float doublePacked2float(uint8_t *bar) {
  _FLOATCONV fl;
  _DBLCONV dbl = {};
  for (int i = 0; i < 8; i++) {
    dbl.b[i] = bar[i];
  }
  fl.p.s = dbl.p.s;
  fl.p.m = dbl.p.m;
  fl.p.e = dbl.p.e + 127 - 1023;  // exponent adjust

  return fl.f;
}

int64_t adjustRange(int64_t input,
    int64_t inMin,
    int64_t inMax,
    int64_t outMin,
    int64_t outMax) {
  int64_t result = (input - inMin) * (outMax - outMin) / (inMax - inMin);
  return result + outMin;
}

bool isArrayEmpty(void* array, size_t arraySize) {
  auto buf = reinterpret_cast<char *>(array);
  for (size_t i = 0; i <  arraySize; i++) {
    if (buf[i] != 0) {
      return false;
    }
  }
  return true;
}

int generateHexString(const void *input,
    char *output,
    int inputLength,
    char separator) {
  const char hexMap[] = "0123456789ABCDEF";
  int destIdx = 0;

  for (int i = 0; i < inputLength; i++) {
    if (i && separator) {
      output[destIdx++] = separator;
    }
    int high = static_cast<const uint8_t *>(input)[i] / 16;
    if (high > 15) {
      high = 15;
    }
    output[destIdx++] = hexMap[high];
    output[destIdx++] = hexMap[static_cast<const uint8_t *>(input)[i] % 16];
  }
  output[destIdx] = 0;
  return destIdx;
}

static bool hexCharToNibble(char hexChar, uint8_t *result) {
  if (hexChar >= 'A' && hexChar <= 'F') {
    *result = static_cast<uint8_t>(hexChar - 'A' + 10);
    return true;
  }
  if (hexChar >= 'a' && hexChar <= 'f') {
    *result = static_cast<uint8_t>(hexChar - 'a' + 10);
    return true;
  }
  if (hexChar >= '0' && hexChar <= '9') {
    *result = static_cast<uint8_t>(hexChar - '0');
    return true;
  }
  return false;
}

bool hexByteToInt(const char *str, uint8_t *result) {
  if (str == nullptr || result == nullptr) {
    return false;
  }

  uint8_t high = 0;
  uint8_t low = 0;
  if (!hexCharToNibble(str[0], &high) || !hexCharToNibble(str[1], &low)) {
    return false;
  }

  *result = static_cast<uint8_t>((high << 4) | low);
  return true;
}

bool hexStringToArray(const char *input, char *output, int outputLength) {
  for (int i = 0; i < outputLength; i++) {
    uint8_t value = 0;
    if (!hexByteToInt(input + 2 * i, &value)) {
      output[i] = 0;
      return false;
    }
    output[i] = static_cast<char>(value);
  }
  return true;
}

uint32_t stringToUInt(const char *str, int len) {
  if (len == -1) {
    len = strlen(str);
  }

  uint64_t result = 0;
  const uint64_t maxValue = UINT32_MAX;

  for (int i = 0; i < len; i++) {
    if (str[i] < '0' || str[i] > '9') {
      SUPLA_LOG_ERROR("stringToUInt: invalid character");
      return 0;
    }
    uint8_t digit = static_cast<uint8_t>(str[i] - '0');
    if (result > (maxValue - digit) / 10) {
      SUPLA_LOG_ERROR("stringToUInt: overflow");
      return 0;
    }
    result = result * 10 + digit;
  }

  return static_cast<uint32_t>(result);
}

int32_t stringToInt(const char *str, int len) {
  if (len == -1) {
    len = strlen(str);
  }

  bool minusFound = false;
  uint64_t result = 0;
  const uint64_t maxValue = static_cast<uint64_t>(INT32_MAX) + 1;

  for (int i = 0; i < len; i++) {
    if (str[i] == '-') {
      if (i == 0) {
        minusFound = true;
        continue;
      } else {
        SUPLA_LOG_ERROR("stringToInt: invalid minus position");
        return 0;
      }
    }
    if (str[i] < '0' || str[i] > '9') {
      SUPLA_LOG_ERROR("stringToInt: invalid character");
      return 0;
    }
    uint8_t digit = static_cast<uint8_t>(str[i] - '0');
    uint64_t signedLimit = minusFound ? maxValue : INT32_MAX;
    if (result > (signedLimit - digit) / 10) {
      SUPLA_LOG_ERROR("stringToInt: overflow");
      return 0;
    }
    result = result * 10 + digit;
  }

  if (minusFound) {
    if (result == maxValue) {
      return INT32_MIN;
    }
    return -static_cast<int32_t>(result);
  }

  return static_cast<int32_t>(result);
}

bool urlDecodeInplace(char *buffer, int size) {
  auto insertPtr = buffer;
  auto parserPtr = buffer;
  auto endPtr = &buffer[size];
  while (parserPtr < endPtr && *parserPtr != '\0') {
    if (*parserPtr == '+') {
      *insertPtr = ' ';
    } else if (*parserPtr == '%') {
      parserPtr++;  // skip '%'
      if (parserPtr + 1 < endPtr) {
        uint8_t decoded = 0;
        if (!hexByteToInt(parserPtr, &decoded)) {
          return false;
        }
        *insertPtr = static_cast<char>(decoded);
        parserPtr++;  // there are 2 bytes, so we shift one here
                      // decode %HEX
      } else {
        *insertPtr = '\0';
        parserPtr = endPtr;
        // invalid character at the end of buffer - may be result of
        // limitted buffer size -> truncate it
      }
    } else {
      *insertPtr = *parserPtr;
    }
    insertPtr++;
    parserPtr++;
  }
  *insertPtr = '\0';
  return true;
}

int urlEncode(const char *input, char *output, int outputMaxSize) {
  auto parserPtr = input;
  auto outputPtr = output;
  auto lastOutputPtr = output + outputMaxSize - 1;
  while (*parserPtr && outputPtr < lastOutputPtr) {
    if ((*parserPtr >= '0' && *parserPtr <= '9')
        || (*parserPtr >= 'A' && *parserPtr <= 'Z')
        || (*parserPtr >= 'a' && *parserPtr <= 'z')
        || *parserPtr == '-'
        || *parserPtr == '_'
        || *parserPtr == '.'
        || *parserPtr == '~') {
      *outputPtr++ = *parserPtr;
    } else {
      if (outputPtr + 3 <= lastOutputPtr) {
        *outputPtr++ = '%';
        outputPtr += generateHexString(parserPtr, outputPtr, 1, 0);
      } else {
        break;
      }
    }
    parserPtr++;
  }
  *outputPtr = '\0';

  return outputPtr - output;
}

int stringAppend(char *output, const char *input, int maxSize) {
  int inputSize = strlen(input);
  if (inputSize < maxSize) {
    memcpy(output, input, inputSize);
    return inputSize;
  }
  return 0;
}

int strncmpInsensitive(const char *s1, const char *s2, int size) {
  if (s1 == s2) {
    return 0;
  }
  if (s1 == nullptr) {
    return -1;
  }
  if (s2 == nullptr) {
    return 1;
  }

  for (int i = 0; i < size; i++) {
    char c1 = s1[i];
    char c2 = s2[i];

    if (c1 >= 97 && c1 <= 122) {
      c1 -= 32;
    }

    if (c2 >= 97 && c2 <= 122) {
      c2 -= 32;
    }

    if (c1 < c2) {
      return -1;
    }

    if (c1 > c2) {
      return 1;
    }
  }

  return 0;
}

// Converts float with the precision specified by decLimit to int multiplied by
// 10 raised to the power of decLimit, so if precision == 2 then "3.1415" -> 314
int32_t floatStringToInt(const char *str, int precision) {
  int32_t result = 0;
  bool minusFound = false;
  int decimalPlaces = -1;
  for (int i = 0; str[i] != 0; i++) {
    if (str[i] == '-') {
      if (i == 0) {
        minusFound = true;
        continue;
      } else {
        return 0;
      }
    }
    if (str[i] >= '0' && str[i] <= '9') {
      result = result * 10 + str[i] - '0';
      if (decimalPlaces >= 0) {
        decimalPlaces++;
      }
    } else if (str[i] == '.' || str[i] == ',') {
      decimalPlaces++;
    }
    if (decimalPlaces >= precision) {
      break;
    }
  }

  if (decimalPlaces < 0) {
    decimalPlaces = 0;
  }

  while (decimalPlaces < precision) {
    result *= 10;
    decimalPlaces++;
  }

  return minusFound ? -result : result;
}

const char *getManufacturer(int16_t id) {
  switch (id) {
    case SUPLA_MFR_ACSOFTWARE: {
      return "AC SOFTWARE";
    }
    case SUPLA_MFR_TRANSCOM: {
      return "TransCom";
    }
    case SUPLA_MFR_LOGI: {
      return "Logi";
    }
    case SUPLA_MFR_ZAMEL: {
      return "Zamel";
    }
    case SUPLA_MFR_NICE: {
      return "Nice";
    }
    case SUPLA_MFR_ITEAD: {
      return "Itead";
    }
    case SUPLA_MFR_DOYLETRATT: {
      return "Doyle & Tratt";
    }
    case SUPLA_MFR_HEATPOL: {
      return "Heatpol";
    }
    case SUPLA_MFR_FAKRO: {
      return "Fakro";
    }
    case SUPLA_MFR_PEVEKO: {
      return "Peveko";
    }
    case SUPLA_MFR_WEKTA: {
      return "Wekta";
    }
    case SUPLA_MFR_STA_SYSTEM: {
      return "STA-System";
    }
    case SUPLA_MFR_DGF: {
      return "DGF";
    }
    case SUPLA_MFR_COMELIT: {
      return "Comelit";
    }
    case SUPLA_MFR_ERGO_ENERGIA: {
      return "ERGO energia";
    }
    case SUPLA_MFR_SOMEF: {
      return "Somef";
    }
    case SUPLA_MFR_AURATON: {
      return "Auraton";
    }
    case SUPLA_MFR_HPD: {
      return "HPD";
    }
  }

  return "Unknown";
}

bool stringToColor(const char *payload,
                   uint8_t *red,
                   uint8_t *green,
                   uint8_t *blue) {
#ifndef ARDUINO_ARCH_AVR
  char *endPtr = nullptr;
  int32_t redL = 0;
  int32_t greenL = 0;
  int32_t blueL = 0;

  redL = std::strtol(payload, &endPtr, 10);
  if (*endPtr != ',') {
    return false;
  }

  ++endPtr;
  greenL = std::strtol(endPtr, &endPtr, 10);
  if (*endPtr != ',') {
    return false;
  }

  ++endPtr;
  blueL = std::strtol(endPtr, &endPtr, 10);
  if (*endPtr != '\0') {
    return false;
  }

  if (redL < 0 || redL > 255 || greenL < 0 || greenL > 255 || blueL < 0 ||
      blueL > 255) {
    return false;
  }

  *red = static_cast<uint8_t>(redL);
  *green = static_cast<uint8_t>(greenL);
  *blue = static_cast<uint8_t>(blueL);

  return true;
#else
  (void)(payload);
  (void)(red);
  (void)(green);
  (void)(blue);
  return false;
#endif
}

int Supla::getBitNumber(uint64_t value) {
  if (value == 0 || (value & (value - 1)) != 0) {
    // more than 1 bit set
    return -1;
  }
  int position = 0;
  while ((value & 1) != 1) {
    value >>= 1;
    position++;
  }

  return position;
}

int Supla::rssiToSignalStrength(int rssi, int rssiZero) {
  const int rssi100Percent = -50;
  if (rssi > rssi100Percent) {
    return 100;
  } else if (rssi <= rssiZero) {
    return 0;
  }

  // map rssi to 0..100% (rssiZero..-50)
  return (rssiZero - rssi) * 100 / (rssiZero - rssi100Percent);
}

const char *Supla::getRelayChannelName(int channelFunction) {
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_POWERSWITCH: {
      return "Power switch";
    }
    case SUPLA_CHANNELFNC_LIGHTSWITCH: {
      return "Light switch";
    }
    case SUPLA_CHANNELFNC_STAIRCASETIMER: {
      return "Staircase timer";
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATE: {
      return "Gate";
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK: {
      return "Door lock";
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR: {
      return "Garage door";
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK: {
      return "Gateway lock";
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
      return "Roller shutter";
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
      return "Roof window";
    }
    case SUPLA_CHANNELFNC_TERRACE_AWNING: {
      return "Terrace awning";
    }
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR: {
      return "Roller garage door";
    }
    case SUPLA_CHANNELFNC_CURTAIN: {
      return "Curtain";
    }
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN: {
      return "Projector screen";
    }
    case SUPLA_CHANNELFNC_VERTICAL_BLIND: {
      return "Vertical blind";
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND: {
      return "Facade blind";
    }
    case SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH: {
      return "Heater/cooling switch";
    }
    case SUPLA_CHANNELFNC_PUMPSWITCH: {
      return "Pump switch";
    }

    default: {
      return "Relay";
    }
  }
}

const char *Supla::getBinarySensorChannelName(int channelFunction) {
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_OPENINGSENSOR_GATEWAY: {
      return "Gateway sensor";
    }
    case SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR: {
      return "Door sensor";
    }
    case SUPLA_CHANNELFNC_OPENINGSENSOR_GATE: {
      return "Gate sensor";
    }
    case SUPLA_CHANNELFNC_OPENINGSENSOR_GARAGEDOOR: {
      return "Garage door sensor";
    }
    case SUPLA_CHANNELFNC_NOLIQUIDSENSOR: {
      return "Liquid sensor";
      break;
    }
    case SUPLA_CHANNELFNC_FLOOD_SENSOR: {
      return "Flood sensor";
      break;
    }
    case SUPLA_CHANNELFNC_CONTAINER_LEVEL_SENSOR: {
      return "Container level sensor";
      break;
    }
    case SUPLA_CHANNELFNC_OPENINGSENSOR_ROLLERSHUTTER: {
      return "Roller shutter sensor";
    }
    case SUPLA_CHANNELFNC_OPENINGSENSOR_ROOFWINDOW: {
      return "Roof window sensor";
    }
    case SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW: {
      return "Window sensor";
      break;
    }
    case SUPLA_CHANNELFNC_HOTELCARDSENSOR: {
      return "Hotel card sensor";
    }
    case SUPLA_CHANNELFNC_ALARMARMAMENTSENSOR: {
      return "Alarm armament sensor";
    }
    case SUPLA_CHANNELFNC_MAILSENSOR: {
      return "Mail sensor";
    }
    case SUPLA_CHANNELFNC_MOTION_SENSOR: {
      return "Motion sensor";
    }
    case SUPLA_CHANNELFNC_BINARY_SENSOR:
    default: {
      return "Binary sensor";
    }
  }
}

bool Supla::isLittleEndian() {
    uint32_t num = 0x01020304;
    return *(reinterpret_cast<uint8_t*>(&num)) == 0x04;
}

int Supla::compareSemVer(const char *sw1, const char *sw2) {
  const char* p1 = sw1;
  const char* p2 = sw2;

  if (p1 == nullptr || p2 == nullptr) {
    return 0;
  }

  while (*p1 != '\0' || *p2 != '\0') {
    int v1 = 0;
    while (*p1 && isdigit(*p1)) {
      v1 = v1 * 10 + (*p1 - '0');
      p1++;
    }
    while (*p1 != '\0' && !isdigit(*p1)) {
      p1++;
    }

    int v2 = 0;
    while (*p2 && isdigit(*p2)) {
      v2 = v2 * 10 + (*p2 - '0');
      p2++;
    }
    while (*p2 != '\0' && !isdigit(*p2)) {
      p2++;
    }

    if (v1 > v2) return 1;
    if (v1 < v2) return -1;
  }

  return 0;
}
