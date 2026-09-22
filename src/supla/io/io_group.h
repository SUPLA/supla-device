// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_IO_IO_GROUP_H_
#define SRC_SUPLA_IO_IO_GROUP_H_

#include <supla/io.h>

namespace Supla {
namespace Io {

// One virtual output backed by several physical pins. Digital writes use each
// member's active/inactive level; digital reads return the first member's
// active state. PWM values stay unchanged; group/member inversions are XORed
// during configuration. PWM ranges/frequencies must be compatible.
//
// The caller owns the array and backends; they must outlive this group and
// remain unchanged while it is in use. No allocation or value cache is used.
// Set mode and pull-up on the virtual IoPin returned by getPin(). Its polarity
// can invert the whole group in addition to each member's own polarity.
// Member mode/pull-up flags are ignored. Writes are sequential, not atomic.
// Use distinct, set pins, with no cyclic group references.
class IoGroup : public Base {
 public:
  IoGroup(const IoPin *pins, uint8_t count);

  // All pin numbers on this backend refer to the same group. Use one handle
  // (pin 0) consistently so existing IoPin equality checks keep working.
  IoPin getPin();

  bool isReady() const override;
  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override;
  void customDigitalWrite(int channelNumber, uint8_t pin, uint8_t val) override;
  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override;
  void customConfigureAnalogOutput(int channelNumber,
                                   uint8_t pin,
                                   bool outputInvert = false) override;
  void customSetPwmResolutionBits(uint8_t pin, uint8_t bits) override;
  void customSetPwmFrequency(uint16_t frequency) override;

  int customDigitalRead(int channelNumber, uint8_t pin) override;
  int customAnalogRead(int channelNumber, uint8_t pin) override;
  unsigned int customPulseIn(int channelNumber,
                             uint8_t pin,
                             uint8_t value,
                             uint64_t timeoutMicro) override;
  uint8_t customDefaultPwmResolutionBits(uint8_t pin) const override;
  bool customCanSetPwmResolutionBits(uint8_t pin) const override;
  uint8_t customPwmResolutionBits(uint8_t pin) const override;
  uint32_t customPwmMaxValue(uint8_t pin) const override;
  uint16_t customPwmFrequency() const override;

  // Interrupts observe the first member, just like reads.
  void customAttachInterrupt(uint8_t pin,
                              void (*func)(void),
                              int mode) override;
  void customDetachInterrupt(uint8_t pin) override;
  uint8_t customPinToInterrupt(uint8_t pin) override;

 private:
  const IoPin *pins;
  uint8_t count;
};

}  // namespace Io
}  // namespace Supla

#endif  // SRC_SUPLA_IO_IO_GROUP_H_
