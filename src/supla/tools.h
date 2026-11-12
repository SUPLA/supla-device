// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_TOOLS_H_
#define SRC_SUPLA_TOOLS_H_

#include <stddef.h>
#include <stdint.h>
#include "definitions.h"

/**
* Converts packed two float to double.
*
* @return (string)bar
*/
void float2DoublePacked(float number, uint8_t *bar, int byteOrder = LSBFIRST);

/**
* Converts double from packed two float to float.
*
* @return float
*/
float doublePacked2float(uint8_t *bar);

/**
* Adjust range from variable.
*
* @return (input - inMin) * (outMax - outMin) / (inMax - inMin)
*/
int64_t adjustRange(int64_t input,
    int64_t inMin,
    int64_t inMax,
    int64_t outMin,
    int64_t outMax);

/**
* Check all variable from array, to find char.
*
* @return true if array is empty
*/
bool isArrayEmpty(void* array, size_t arraySize);

/**
* Converts inputLength bytes from input to HEX and adds bytes separator
* if required.
* output buffor has to be at least (2 * inputLength + 1) bytes long without
* separator, or: (3 * inputLength) bytes long with separator.
* Trailing '\0' is added.
*
* @return amount of non-null chars written
*/
int generateHexString(const void *input,
    char *output,
    int inputLength,
    char separator = 0);

/**
* Converts hex byte string value to integer.
*
* @return false when input contains invalid hex characters
*/
bool hexByteToInt(const char *str, uint8_t *result);

/**
* Converts hex string to array of bytes.
*
* @return false when input contains invalid hex characters.
*/
bool hexStringToArray(const char *input, char *output, int outputLength);

/**
* Converts decimal string value to unsigned integer.
*
* @return unsigned integer
*/
uint32_t stringToUInt(const char *str, int len = -1);

/**
* Converts decimal string value to signed integer.
*
* @return signed integer
*/
int32_t stringToInt(const char *str, int len = -1);

/**
* Converts float with the precision specified by decLimit to int multiplied by
* 10 raised to the power of decLimit, so if precision == 2 then "3.1415" -> 314.
*
* @return signed integer
*/
int32_t floatStringToInt(const char *str, int precision);

/**
* Converts "230,12,43" string to RGB values.
*
* @return false on error
*/
bool stringToColor(const char *payload,
                   uint8_t *red,
                   uint8_t *green,
                   uint8_t *blue);

/**
* Decode url string from buffer into buffer (inplace)
* Replace '+' with ' '.
* Replace %xy with proper byte.
* If not complete % parameter is found at the end, then it is omitted.
*
* @return false when invalid hex escape is found
*/
bool urlDecodeInplace(char *buffer, int size);

/**
* Encode url string from input to output.
*
* @return number of non-null bytes added to output
*/
int urlEncode(const char *input, char *output, int outputMaxSize);

/**
* Append two strings.
*
* @return size of string
*/
int stringAppend(char *output, const char *input, int maxSize);

/**
* Compare two character strings.
*
* @return 0 if strings is the same
*/
int strncmpInsensitive(const char *s1, const char *s2, int size);

/**
 * Do software reset device.
 */
void deviceSoftwareReset();

/**
 * Checks support software reset from device.
 *
 * @return true if device support software reset
 */
bool isDeviceSoftwareResetSupported();

/**
 * Checks last software reset.
 *
 * @return true if device be software reset
 */
bool isLastResetSoft();

/**
 * Checks name of manufacturer.
 *
 * @return name(string) of manufacturer
 */
const char *getManufacturer(int16_t id);

namespace Supla {

/**
* Checks name of platform ID (not supported yet).
*
* @return 0
*/
int getPlatformId();

/**
 * Find where is true bit from value.
 *
 * @return position true bit
 */
int getBitNumber(uint64_t value);

/**
 * Convert RSSI to signal strength.
 *
 * @return signal strength
 */
int rssiToSignalStrength(int rssi, int rssiZero = -100);

/**
 * Check last reset power.
 *
 * @return true if be last reset power
 */
bool isLastResetPower();

/**
 * Checks name of relay.
 *
 * @return name(string) of relay
 */
const char *getRelayChannelName(int channelFunction);

/**
 * Checks name of binary sensor.
 *
 * @return name(string) of binary sensor
 */
const char *getBinarySensorChannelName(int channelFunction);

/**
 * Checks if current platform is little endian byte order
 *
 * @return true if current platform is little endian
 */
bool isLittleEndian();

/**
 * Compares two semantic versions
 *
 * @param sw1 Software version 1 to compare
 * @param sw2 Software version 2 to compare
 *
 * @return 1 if sw1 > sw2, 0 if sw1 == sw2, -1 if sw1 < sw2
 */
int compareSemVer(const char *sw1, const char *sw2);

/**
 * Fills buffer with random bytes
 *
 * @param buffer
 * @param size
 */
#ifndef ARDUINO_ARCH_AVR
void fillRandom(uint8_t *buffer, int size);
#endif

}  // namespace Supla


#endif  // SRC_SUPLA_TOOLS_H_
