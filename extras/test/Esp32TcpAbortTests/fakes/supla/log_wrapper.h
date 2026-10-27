// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_SUPLA_LOG_WRAPPER_H_
#define EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_SUPLA_LOG_WRAPPER_H_

extern "C" void fakeLwipLog(const char *format, ...);
#define SUPLA_LOG_DEBUG(...) fakeLwipLog(__VA_ARGS__)
#define SUPLA_LOG_WARNING(...) fakeLwipLog(__VA_ARGS__)

#endif  // EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_SUPLA_LOG_WRAPPER_H_
