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

#ifndef SRC_SUPLA_NETWORK_HTML_SELECT_INPUT_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_SELECT_INPUT_PARAMETER_H_

#include <supla/network/html_element.h>
#include <supla/local_action.h>
#include <stdint.h>

namespace Supla {

namespace Html {

struct SelectValueMapElement {
  char *name = nullptr;
  int value = 0;
  SelectValueMapElement *next = nullptr;

  ~SelectValueMapElement();
};

class SelectInputParameter : public HtmlElement, public LocalAction {
 public:
  SelectInputParameter();
  SelectInputParameter(const char *paramTag, const char *paramLabel);
  virtual ~SelectInputParameter();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void registerValue(const char *name, int value);
  void setTag(const char *tagValue);
  void setLabel(const char *labelValue);
  void onProcessingEnd() override;

  void setBaseTypeBitCount(uint8_t value);

 protected:
  char *tag = nullptr;
  char *label = nullptr;
  SelectValueMapElement *firstValue = nullptr;
  bool configChanged = false;
  uint8_t baseTypeBitCount = 32;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_SELECT_INPUT_PARAMETER_H_
