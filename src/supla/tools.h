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

#ifndef SRC_SUPLA_TOOLS_H_
#define SRC_SUPLA_TOOLS_H_

#include <stddef.h>
#include <stdint.h>
#include "definitions.h"
#include "supla/IEEE754tools.h"

void float2DoublePacked(float number, uint8_t *bar, int byteOrder = LSBFIRST);
float doublePacked2float(uint8_t *bar);

int64_t adjustRange(int64_t input,
    int64_t inMin,
    int64_t inMax,
    int64_t outMin,
    int64_t outMax);

bool isArrayEmpty(void* array, size_t arraySize);

// Converts inputLength bytes from input to HEX and adds bytes separator
// if required.
// output buffor has to be at least (2 * inputLength + 1) bytes long without
// separator, or: (3 * inputLength) bytes long with separator.
// Trailing '\0' is added.
// Returns amount of non-null chars written.
int generateHexString(const void *input,
    char *output,
    int inputLength,
    char separator = 0);

void hexStringToArray(const char *input, char *output, int outputLength);

// Converts hex string value to integer
uint32_t hexStringToInt(const char *str, int len = -1);

// Converts decimal string value to unsigned integer
uint32_t stringToUInt(const char *str, int len = -1);

// Converts decimal string value to signed integer
int32_t stringToInt(const char *str, int len = -1);

// Convers float value from string to integer with given precision
int32_t floatStringToInt(const char *str, int precision);

// Converts "230,12,43" string to RGB values. Returns false on error
bool stringToColor(const char *payload,
                   uint8_t *red,
                   uint8_t *green,
                   uint8_t *blue);

// Decode url string from buffer into buffer (inplace)
// Replace '+' with ' '.
// Replace %xy with proper byte.
// If not complete % parameter is found at the end, then it is omitted.
void urlDecodeInplace(char *buffer, int size);

// Encode url string from input to output
// Returns number of non-null bytes added to output
int urlEncode(const char *input, char *output, int outputMaxSize);

int stringAppend(char *output, const char *input, int maxSize);

int strncmpInsensitive(const char *s1, const char *s2, int size);

// This method should be implemented in platform specific cpp file
void deviceSoftwareReset();
bool isDeviceSoftwareResetSupported();
bool isLastResetSoft();

const char *getManufacturer(int16_t id);

namespace Supla {
int getPlatformId();
int getBitNumber(uint64_t value);
int rssiToSignalStrength(int rssi);
bool isLastResetPower();

const char *getRelayChannelName(int channelFunction);
const char *getBinarySensorChannelName(int channelFunction);
}  // namespace Supla


#endif  // SRC_SUPLA_TOOLS_H_
