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

#ifndef EXTRAS_ESP_IDF_SUPLA_MCP23008_SUPLA_MCP23008_H_
#define EXTRAS_ESP_IDF_SUPLA_MCP23008_SUPLA_MCP23008_H_

#include <supla/io.h>
#include <esp_i2c_driver.h>
#include <driver/i2c_master.h>

namespace Supla {

/**
 * MCP23008 I/O expander for esp-idf.
 */
class MCP23008 : public Supla::Io::Base {
 public:
  enum class Register : uint8_t {
    IODIR = 0x00,    /// I/O direction: bitmap of 1=input, 0=output
    IPOL = 0x01,     /// Input polarity: bitmap of 0=normal, 1=inverted
    GPINTEN = 0x02,  /// Interrupt on change: bitmap of 1=enable, 0=disable
    DEFVAL = 0x03,   /// Default compare value for interrupt
    INTCON = 0x04,   /// Interrupt control
    IOCON = 0x05,    /// Configuration
    GPPU = 0x06,     /// Pull-up enable: bitmap of 1=enable, 0=disable
    INTF = 0x07,     /// Interrupt flag
    INTCAP = 0x08,   /// Interrupt capture
    GPIO = 0x09,     /// GPIO state
    OLAT = 0x0A      /// Output latch
  };

  enum class IoDir : uint8_t {
    Output = 0,
    Input = 1,
  };

  enum class Configuration : uint8_t {
    SequentialOperationDisabled = (1 << 5),
    SlewStateDisabled = (1 << 4),
    OpenDrainDisabled = (1 << 2),
    InterruptPolarityActiveHigh = (1 << 1),
  };

  explicit MCP23008(Supla::I2CDriver *driver, uint8_t address = 0b0100000,
                    bool initDefaults = false);

  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override;
  int customDigitalRead(int channelNumber, uint8_t pin) override;
  void customDigitalWrite(int channelNumber, uint8_t pin, uint8_t val) override;

  unsigned int customPulseIn(int channelNumber,
                             uint8_t pin,
                             uint8_t value,
                             uint64_t timeoutMicro) override;
  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override;
  int customAnalogRead(int channelNumber, uint8_t pin) override;

  /**
   * Initialize the MCP23008
   *
   * @return true on success, false otherwise
   */
  bool init();

 private:
  void readAllAndPrint();
  void writeMode();
  void writeState();
  void readState();

  Supla::I2CDriver *driver = nullptr;
  i2c_master_dev_handle_t handle = nullptr;
  uint8_t address = 0b0100000;
  uint8_t mode = 0;
  uint8_t pullup = 0;
  uint8_t state = 0;
  bool initDefaults = false;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_MCP23008_SUPLA_MCP23008_H_
