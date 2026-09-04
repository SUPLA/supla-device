// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_SSD1306_SSD1306_H_
#define EXTRAS_ESP_IDF_SUPLA_SSD1306_SSD1306_H_

#include <stddef.h>
#include <stdint.h>

#include <driver/i2c_types.h>
#include <esp_i2c_driver.h>

namespace Supla {
namespace Display {

class SSD1306 {
 public:
  static constexpr int WIDTH = 128;
  static constexpr int HEIGHT = 64;

  explicit SSD1306(Supla::I2CDriver *driver,
                   uint8_t address = 0x3C,
                   uint32_t frequency = 400000);

  bool initialize();
  bool isInitialized() const;
  bool isOn() const;
  void displayOn();
  void displayOff();
  void setContrast(uint8_t contrast);

  void clear();
  void text(int y, int x, const char *value);
  void smallText(int page, int column, const char *value);
  void pixel(int x, int y, bool on);
  void rect(int x, int y, int width, int height);
  void fillRect(int x, int y, int width, int height);
  void stateDot(int x, int y, bool active);
  void progressBar(int y, uint8_t percent);
  void flush();

 protected:
  static constexpr int DISPLAY_PAGES = HEIGHT / 8;

  bool flushRegion(int firstPage,
                   int pageCount,
                   int firstColumn,
                   int columnCount);
  bool sendCommand(uint8_t command);
  bool sendData(const uint8_t *data, size_t size);

  uint8_t buffer[WIDTH * HEIGHT / 8] = {};

 private:
  void drawChar7x9(int y, int x, char value);
  void drawChar5x7(int page, int column, char value);

  uint8_t address = 0;
  uint32_t frequency = 0;
  bool initialized = false;
  bool screenOn = false;
  Supla::I2CDriver *driver = nullptr;
  i2c_master_dev_handle_t devHandle = nullptr;
};

}  // namespace Display
}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_SSD1306_SSD1306_H_
