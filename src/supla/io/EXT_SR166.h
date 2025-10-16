
#ifndef SRC_SUPLA_CONTROL_EXT_SR166_H_
#define SRC_SUPLA_CONTROL_EXT_166_H_

#include <supla/io.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Control {

class ExtSR166 : public Supla::Io {
  public:
    explicit ExtSR166(const uint8_t size,
                      const uint8_t dataPin,
                      const uint8_t clockPin,
                      const uint8_t ploadPin) :
      _size(size), _clockPin(clockPin), _dataPin(dataPin), _ploadPin(ploadPin) , Supla::Io(false) {

      pinMode(_clockPin,        OUTPUT);
      pinMode(_dataPin,          INPUT);
      pinMode(_ploadPin,        OUTPUT);
      digitalWrite(_clockPin,      LOW);
      digitalWrite(_ploadPin,     HIGH);

      _digitalValue.resize(_size, 0);
    }

    void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {
    }
    void customDigitalWrite(int channelNumber, uint8_t pin,
                            uint8_t val) override {
    }
    int customDigitalRead(int channelNumber, uint8_t pin) override {
      if (millis() - lastSrRead >= 10) {
        readSR();
        lastSrRead = millis();
      }
      return (_digitalValue[pin / 8] >> (pin % 8)) & 1;
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
    void readSR() {
      digitalWrite(_ploadPin, LOW);
      delayMicroseconds(pulseWidth);
      digitalWrite(_clockPin, HIGH);   // HC166
      delayMicroseconds(pulseWidth);   // HC166
      digitalWrite(_clockPin, LOW);    // HC166
      digitalWrite(_ploadPin, HIGH);

      for (int i = _size - 1; i >= 0; i--) {
        _digitalValue[i] = 0;
        for (int j = 7; j >= 0; j--) {
          bool temp = digitalRead(_dataPin);
          if (temp)  _digitalValue[i] = _digitalValue[i] | (1 << j);
          digitalWrite(_clockPin, HIGH);
          delayMicroseconds(pulseWidth);
          digitalWrite(_clockPin, LOW);
        }
      }
    }

  protected:
    uint8_t _size;
    uint8_t _clockPin;
    uint8_t _dataPin;
    uint8_t _ploadPin;
    uint8_t pulseWidth = 5;
    unsigned long lastSrRead;

    std::vector<uint8_t>_digitalValue;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_EXT_SR166_H_