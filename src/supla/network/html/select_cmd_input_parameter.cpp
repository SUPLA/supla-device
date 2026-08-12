// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
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

#endif  // ARDUINO_ARCH_AVR
