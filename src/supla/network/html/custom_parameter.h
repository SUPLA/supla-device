/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef SRC_SUPLA_NETWORK_HTML_CUSTOM_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_CUSTOM_PARAMETER_H_

#include <supla/network/html_element.h>
#include <stdint.h>

namespace Supla {

namespace Html {

/* This HTML Element provides input in config mode for integer value.
 * You have to provide paramTag under which provided value will be stored
 * in Supla::Storage::Config.
 * paramLabel provides label which is displayed next to input in www.
 */

class CustomParameter : public HtmlElement {
 public:
  CustomParameter(const char *paramTag,
                  const char *paramLabel,
                  int32_t defaultValue = 0);
  virtual ~CustomParameter();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

  int32_t getParameterValue();
  void setParameterValue(const int32_t);

 protected:
  int32_t parameterValue = 0;
  char *tag = nullptr;
  char *label = nullptr;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_CUSTOM_PARAMETER_H_
