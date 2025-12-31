/*
   Copyright (C) malarz

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
