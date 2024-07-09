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

#ifndef SRC_SUPLA_CONTROL_EXT_SR595_H_
#define SRC_SUPLA_CONTROL_EXT_SR595_H_

#include <supla/io.h>
//#include <supla/log_wrapper.h>

namespace Supla {
namespace Control {

class ExtSR595 : public Supla::Io {
  public:
    explicit ExtSR595(const uint8_t numSr,
                      const uint8_t serialDataPin,
                      const uint8_t clockPin,
                      const uint8_t latchPin) :
      _numSr(numSr), _clockPin(clockPin), _serialDataPin(serialDataPin), _latchPin(latchPin) , Supla::Io(false) {

      pinMode(_clockPin,      OUTPUT);
      pinMode(_serialDataPin, OUTPUT);
      pinMode(_latchPin,      OUTPUT);

      digitalWrite(_clockPin,      LOW);
      digitalWrite(_serialDataPin, LOW);
      digitalWrite(_latchPin,      LOW);

      _sr595State.resize(_numSr, 0);
    }

    void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {
    }
    void customDigitalWrite(int channelNumber, uint8_t pin,
                            uint8_t val) override {
      (val) ? bitSet(_sr595State[pin / 8], pin % 8) : bitClear(_sr595State[pin / 8], pin % 8);
      update595Registers();
    }
    int customDigitalRead(int channelNumber, uint8_t pin) override {
      return (_sr595State[pin / 8] >> (pin % 8)) & 1;
    }
    unsigned int customPulseIn(int channelNumber, uint8_t pin, uint8_t value,
                               uint64_t timeoutMicro) override {
      return 0;
    }
    void customAnalogWrite(int channelNumber, uint8_t pin, int val) override {
    }

    int customAnalogRead(int channelNumber, uint8_t pin) override {
      return 0;
    }
    void update595Registers() {
      for (int i = _numSr - 1; i >= 0; i--) {
        shiftOut(_serialDataPin, _clockPin, MSBFIRST, _sr595State[i]);
      }
      digitalWrite(_latchPin, HIGH);
      digitalWrite(_latchPin, LOW);
    }

  protected:
    uint8_t _numSr;
    uint8_t _clockPin;
    uint8_t _serialDataPin;
    uint8_t _latchPin;

    std::vector<uint8_t>_sr595State;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_EXT_SR595_H_