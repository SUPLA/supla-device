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

#ifndef EXTRAS_TEST_DOUBLES_SIMPLE_TIME_H_
#define EXTRAS_TEST_DOUBLES_SIMPLE_TIME_H_

#include <stdint.h>
#include "arduino_mock.h"

class SimpleTime : public TimeInterface {
 public:
  uint32_t millis() override;
  void advance(int advanceMs);

  uint32_t value = 0;
};

#endif  // EXTRAS_TEST_DOUBLES_SIMPLE_TIME_H_
