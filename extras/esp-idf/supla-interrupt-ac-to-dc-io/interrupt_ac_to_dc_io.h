/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifndef EXTRAS_ESP_IDF_SUPLA_INTERRUPT_AC_TO_DC_IO_INTERRUPT_AC_TO_DC_IO_H_
#define EXTRAS_ESP_IDF_SUPLA_INTERRUPT_AC_TO_DC_IO_INTERRUPT_AC_TO_DC_IO_H_

#include <supla/io.h>
#include <supla/element.h>

namespace Supla {

#define INTERRUPT_AC_TO_DC_IO_MAX_GPIOS 50

class InterruptAcToDcIo : public Io, public Element {
 public:
  InterruptAcToDcIo();
  ~InterruptAcToDcIo();

  void initialize();
  bool isInitialized() const;

  void addGpio(int gpio, int32_t minOffTimeoutMs);

  int customDigitalRead(int channelNumber, uint8_t pin) override;
  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override;

  void onFastTimer() override;

 protected:
//  bool gpioHiIsOn[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  int32_t gpioMinOffTimeout[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  int32_t gpioLastTimestampMs[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  uint8_t gpioState[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  bool initialized = false;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_INTERRUPT_AC_TO_DC_IO_INTERRUPT_AC_TO_DC_IO_H_
