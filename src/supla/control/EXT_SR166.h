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

#ifndef SRC_SUPLA_CONTROL_EXT_SR166_H_
#define SRC_SUPLA_CONTROL_EXT_166_H_

#include <supla/io.h>
//#include <supla/log_wrapper.h>

namespace Supla {
namespace Control {

class ExtSR166 : public Supla::Io {
  public:
    explicit ExtSR166(const uint8_t numSr,
                      const uint8_t dataPin,
                      const uint8_t clockPin,
                      const uint8_t loadPin) :
      _numSR(numSr), _clockPin(clockPin), _dataPin(dataPin), _loadPin(loadPin) , Supla::Io(false) {

      pinMode(_clockPin,        OUTPUT);
      pinMode(_dataPin,          INPUT);
      pinMode(_loadPin,         OUTPUT);
      digitalWrite(_clockPin,      LOW);
      digitalWrite(_loadPin,      HIGH);

      _sr166State.resize(_numSR, 0);
    }

    void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {
    }
    void customDigitalWrite(int channelNumber, uint8_t pin,
                            uint8_t val) override {
    }
    int customDigitalRead(int channelNumber, uint8_t pin) override {
      if (lastSrRead == 0 || millis() - lastSrRead >= 10) {
        readSR166();
        lastSrRead = millis();
      }
      return (_sr166State[pin / 8] >> (pin % 8)) & 1;
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
    void readSR166() {
      digitalWrite(_loadPin, LOW);
      delayMicroseconds(pulseWidth);
      digitalWrite(_clockPin, HIGH);   // HC166
      delayMicroseconds(pulseWidth);   // HC166
      digitalWrite(_clockPin, LOW);    // HC166
      digitalWrite(_loadPin, HIGH);

      for (int i = _numSR - 1; i >= 0; i--) {
        _sr166State[i] = 0;
        for (int j = 7; j >= 0; j--) {
          bool temp = digitalRead(_dataPin);
          if (temp)  _sr166State[i] = _sr166State[i] | (1 << j);
          digitalWrite(_clockPin, HIGH);
          delayMicroseconds(pulseWidth);
          digitalWrite(_clockPin, LOW);
        }
      }
    }

  protected:
    uint8_t _numSR;
    uint8_t _clockPin;
    uint8_t _dataPin;
    uint8_t _loadPin;
    uint8_t pulseWidth = 5;
    uint32_t lastSrRead = 0;

    std::vector<uint8_t>_sr166State;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_EXT_SR166_H_