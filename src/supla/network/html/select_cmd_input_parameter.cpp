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

#include "select_cmd_input_parameter.h"
#include <supla/network/web_sender.h>
#include "supla/network/html/text_cmd_input_parameter.h"

Supla::Html::SelectCmdInputParameter::SelectCmdInputParameter(
    const char *paramTag, const char *paramLabel) :
  Supla::Html::TextCmdInputParameter(paramTag, paramLabel) {
}

Supla::Html::SelectCmdInputParameter::~SelectCmdInputParameter() {
}

void Supla::Html::SelectCmdInputParameter::send(Supla::WebSender* sender) {
  // form-field BEGIN
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(tag, label);
  sender->send("<select ");
  sender->sendNameAndId(tag);
  sender->send(">");
  sender->send("<option selected value></option>");
  auto ptr = firstCmd;
  while (ptr) {
    sender->send("<option value=\"");
    sender->send(ptr->cmd);
    sender->send("\">");
    sender->send(ptr->cmd);
    sender->send("</option>");
    ptr = ptr->next;
  }
  sender->send("</select>");
  sender->send("</div>");
  // form-field END
}

