// SPDX-FileCopyrightText: AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_MCP23008TESTS_MCP23008_TEST_SUPPORT_H_
#define EXTRAS_TEST_MCP23008TESTS_MCP23008_TEST_SUPPORT_H_

#include <stdint.h>

namespace MCP23008TestSupport {
void reset();
void setReadValue(uint8_t value);
int getI2cAccessCount();
}  // namespace MCP23008TestSupport

#endif  // EXTRAS_TEST_MCP23008TESTS_MCP23008_TEST_SUPPORT_H_
