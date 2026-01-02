/*
 Copyright (C) AC SOFTWARE SP. Z O.O., malarz

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

#ifndef XMAS_HTML_GENERATOR_H_
#define XMAS_HTML_GENERATOR_H_

#include <supla/network/html_generator.h>

const char xMaslogoSvg[] =
  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"2"
  "00\" fill=\"none\">"
  "<path fill=\"#000\" fill-rule=\"evenodd\" d=\"M69 42a34 34 0 0 1 3"
  "4 34 30 30 0 0 1-1 9v1l30 19 7 5a1 1 0 0 0 2 0c3-3 8-5 12-5a20 20 "
  "0 0 1 20 16 19 19 0 0 1-9 19 20 20 0 0 1-21 1h-1l-11 12-12 13-5 5v"
  "1c3 3 5 6 5 10a16 16 0 0 1-7 15c-9 8-23 3-27-8a17 17 0 0 1 7-19v-1"
  "a1 1 0 0 0 0-1l-5-15a22360 22360 0 0 0-16-44 35 35 0 0 1-26-9c-6-5"
  "-10-13-11-21a32 32 0 0 1 8-25 34 34 0 0 1 24-12h3zm13 65a11547 115"
  "47 0 0 0 15 60 18 18 0 0 1 13 2h1v-1l26-32v-1c-4-6-5-13-1-20v-1l-2"
  "1-12-16-9-2 1-3 4-11 8-1 1zm10-32-1-7c-4-9-11-14-21-15a23 23 0 0 0"
  "-21 34 24 24 0 0 0 38 2c3-4 5-9 5-14zm10 121 2-1a12 12 0 0 0 10-10"
  "l-1-5a12 12 0 0 0-21-3 12 12 0 0 0-2 9 12 12 0 0 0 12 10zm51-83h-2"
  "a11 11 0 0 0-8 15 12 12 0 0 0 21 2c1-3 2-6 1-8a12 12 0 0 0-12-9z\""
  " clip-rule=\"evenodd\"/>"
  "<path fill=\"#efe7d5\" stroke=\"#000\" stroke-width=\"3.8\" d=\"m3"
  "8 5 6-3h4l3 2 2 2 2 8c-1 2-2 5-5 6-2 1-4 2-6 1-3 0-7-1-9-4l-1-4c-1"
  "-1 0-3 1-4l1-2V6l2-1z\"/>"
  "<path fill=\"#d12f00\" stroke=\"#000\" stroke-width=\"3.8\" d=\"m4"
  "3 20 6-1 2-1h6l3 1 5 3h1c1 2 3 2 5 3l6 4 4 3 5 3v1h1l5 5 1 1 1 1v1"
  "l1 1v1h1v2l1 2v1a2 2 0 0 1-1 2l-1 1h-7l1 3v2h-2a84 84 0 0 0-50 14l"
  "-1 1-2 1-3 2h-2v-1l-2-3-1-4v-9a60 60 0 0 1 1-7l1-3 2-5 2-4 1-2a13 "
  "13 0 0 1 3-7l1-3 1-1 1-3 2-2 2-3zm51 32v1-1z\"/>"
  "<path fill=\"#efe7d5\" stroke=\"#000\" stroke-width=\"3.8\" d=\"M6"
  "9 48c4-2 7-2 11-1h2l2 1h4l1-1 3-1h2l3 1 2 3v2l1 1 3 3-1 5c0 2-2 3-"
  "4 3l-5 1c-4-1-8 0-12 2l-3 2-3-1h-2l-2 1-3 2h-6l-3 1-4 1h-3v1h-1v1l"
  "-4 3-2 1-3 2-3 3-4 1-4-1-6-6c-1-2-2-5-1-8l4-5 2-1h1l3-1 3-1 3-2 4-"
  "3 2-2 3-1h1l1-1h2l2-2h6l4-1 4-2z\"/>"
  "<path fill=\"#d12f00\" stroke=\"#000\" stroke-width=\"3.8\" d=\"m8"
  "8 138-7 12-4 4H64l-5-1-2-1v-4l8-13 3-4 1-2v-1l1-5-1-3c-1-2-3-5-2-1"
  "0l5-4a21 21 0 0 1 14 0l2 2 1 3v6l1 9-2 12z\"/>"
  "<path fill=\"#d12f00\" stroke=\"#000\" stroke-width=\"3.8\" d=\"M7"
  "2 123h-6l-1-1h-1l-1-1a79 79 0 0 1-3-2l-1-2-1-2h-1l-2-3v-4l3-2 7-1 "
  "6-2 8-3 8-2c3-1 6-2 7-4 2-1 3-3 4-6l1-4 1-2 2-2h4l5 2 1 2 5 4 2 4a"
  "4 4 0 0 1-2 4l-2 1-1 3v3l-2 3-3 2-1 1-3 2c-3 3-7 4-11 5l-7 2-7 2-5"
  " 2-3 1z\"/>"
  "<path stroke=\"#000\" stroke-linecap=\"round\" stroke-width=\"3.8"
  "\" d=\"m60 154-2 4m8-3-2 3m7-3-1 4m10-8-4 8m-20-11-4 7\"/></svg>";

class xMasHtmlGenerator : public Supla::HtmlGenerator {
 public:
  void sendLogo(Supla::WebSender *sender) {
    sender->send(xMaslogoSvg, strlen(xMaslogoSvg));
  }

};

#endif  // XMAS_HTML_GENERATOR_H_