// SPDX-FileCopyrightText: malarz
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_I2CSCANNER_H_
#define SRC_SUPLA_NETWORK_HTML_I2CSCANNER_H_

#include <Wire.h>
#include <stdio.h>

namespace Supla {
namespace Html {

/**
 * HTML element that will scan I2C bus on each web page load and
 * display all detected addresses on a web page.
 *
 * Prerequisites: use it after Wire.begin
 */
class I2Cscanner : public Supla::HtmlElement {
 public:
  I2Cscanner() : HtmlElement(HTML_SECTION_DEVICE_INFO) {}

  void send(Supla::WebSender* sender) {
    sender->send("<span>");
    sender->send("<br>I2C:");
    for (uint8_t address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      uint8_t error = Wire.endTransmission();
      char buffer[6];
      if (error == 0) {
        snprintf(buffer, sizeof(buffer), " 0x%2x", address);
        sender->send(buffer, 5);
      }
    }
    sender->send("</span>");
  }
};  // I2Cscanner

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_I2CSCANNER_H_
