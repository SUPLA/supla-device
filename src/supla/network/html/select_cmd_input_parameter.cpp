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
  sender->labeledField(tag, label, [&]() {
    sender->selectTag(tag, tag).body([&]() {
      sender->tag("option").attrIf("selected", true).body("");
      auto ptr = firstCmd;
      while (ptr) {
        sender->tag("option").attr("value", ptr->cmd).body(ptr->cmd);
        ptr = ptr->next;
      }
    });
  });
}
