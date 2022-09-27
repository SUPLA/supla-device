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

#ifndef SRC_SUPLA_LOG_WRAPPER_H_
#define SRC_SUPLA_LOG_WRAPPER_H_

#include <stdarg.h>

#include <supla-common/log.h>

// add declaration of methods defined in log.c, but not exposed to log.h
extern "C" char supla_log_string(char **buffer, int *size, va_list va,
                                       const char *__fmt);
extern "C" void supla_vlog(int __pri, const char *message);


#ifdef ARDUINO
#include <Arduino.h>

class __FlashStringHelper;

void supla_logf(int __pri, const __FlashStringHelper *__fmt, ...);

#else

#define F(argument_F) (argument_F)
#define supla_logf supla_log

#endif

// #define SUPLA_DISABLE_LOGS

#ifdef SUPLA_DISABLE_LOGS
// uncomment below lines to disable certain logs
#define SUPLA_LOG_VERBOSE(arg_format, ...) {};
#define SUPLA_LOG_DEBUG(arg_format, ...) {};
#define SUPLA_LOG_INFO(arg_format, ...) {};
#define SUPLA_LOG_WARNING(arg_format, ...) {};
#define SUPLA_LOG_ERROR(arg_format, ...) {};
#endif


#ifndef SUPLA_LOG_VERBOSE
#define SUPLA_LOG_VERBOSE(arg_format, ...) \
            supla_logf(LOG_VERBOSE, F(arg_format) , ## __VA_ARGS__)
#endif

#ifndef SUPLA_LOG_DEBUG
#define SUPLA_LOG_DEBUG(arg_format, ...) \
            supla_logf(LOG_DEBUG, F(arg_format) , ## __VA_ARGS__)
#endif

#ifndef SUPLA_LOG_INFO
#define SUPLA_LOG_INFO(arg_format, ...) \
            supla_logf(LOG_INFO, F(arg_format) , ## __VA_ARGS__)
#endif

#ifndef SUPLA_LOG_WARNING
#define SUPLA_LOG_WARNING(arg_format, ...) \
            supla_logf(LOG_WARNING, F(arg_format) , ## __VA_ARGS__)
#endif

#ifndef SUPLA_LOG_ERROR
#define SUPLA_LOG_ERROR(arg_format, ...) \
            supla_logf(LOG_ERR, F(arg_format) , ## __VA_ARGS__)
#endif

#endif  // SRC_SUPLA_LOG_WRAPPER_H_
