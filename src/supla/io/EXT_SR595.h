
#ifndef SRC_SUPLA_CONTROL_EXT_SR595_H_
#define SRC_SUPLA_CONTROL_EXT_SR595_H_

#include <supla/io.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Control {

class ExtSR595 : public Supla::Io {
  public:
    explicit ExtSR595(const uint8_t size,
                      const uint8_t serialDataPin,
                      const uint8_t clockPin,
                      const uint8_t latchPin) :
      _size(size), _clockPin(clockPin), _serialDataPin(serialDataPin), _latchPin(latchPin) , Supla::Io(false) {

      pinMode(_clockPin,      OUTPUT);
      pinMode(_serialDataPin, OUTPUT);
      pinMode(_latchPin,      OUTPUT);

      digitalWrite(_clockPin,      LOW);
      digitalWrite(_serialDataPin, LOW);
      digitalWrite(_latchPin,      LOW);

      _digitalValues.resize(_size, 0);
    }


    void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {
    }
    void customDigitalWrite(int channelNumber, uint8_t pin,
                            uint8_t val) override {
      (val) ? bitSet(_digitalValues[pin / 8], pin % 8) : bitClear(_digitalValues[pin / 8], pin % 8);
      updateRegisters();
    }
    int customDigitalRead(int channelNumber, uint8_t pin) override {
      return (_digitalValues[pin / 8] >> (pin % 8)) & 1;
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
    void updateRegisters() {
      for (int i = _size - 1; i >= 0; i--) {
        shiftOut(_serialDataPin, _clockPin, MSBFIRST, _digitalValues[i]);
      }
      digitalWrite(_latchPin, HIGH);
      digitalWrite(_latchPin, LOW);
    }

  protected:
    uint8_t _size;
    uint8_t _clockPin;
    uint8_t _serialDataPin;
    uint8_t _latchPin;

    std::vector<uint8_t>_digitalValues;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_EXT_SR595_H_