// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "security_log_list.h"

#include <supla/device/security_logger.h>
#include <supla/network/web_sender.h>

using Supla::Html::SecurityLogList;

SecurityLogList::SecurityLogList(Supla::Device::SecurityLogger* logger)
    : HtmlElement(HTML_SECTION_LOGS), logger(logger) {
}

SecurityLogList::~SecurityLogList() {
}

void SecurityLogList::send(Supla::WebSender* sender) {
  sender->tag("div").attr("class", "card").body([&]() {
    sender->tag("header").body(
        [&]() { sender->tag("h1").body("Security log"); });

    if (logger == nullptr) {
      sender->tag("p").body("No security log");
      return;
    }

    sender->tag("main").body([&]() {
      sender->tag("table").attr("aria-label", "Security log").body([&]() {
        sender->tag("thead").body([&]() {
          sender->tag("tr").body([&]() {
            sender->tag("th").attr("class", "col-num").body("#");
            sender->tag("th").attr("class", "col-ts").body("Timestamp");
            sender->tag("th").attr("class", "col-src").body("Source");
            sender->tag("th").attr("class", "col-msg").body("Message");
          });
        });
        sender->tag("tbody").attr("id", "log-body").body([&]() {
          Supla::SecurityLogEntry* entry = nullptr;
          int count = 0;
          logger->prepareGetLog();
          while ((entry = logger->getLog()) != nullptr) {
            if (entry->isEmpty()) {
              continue;
            }
            count++;
            sender->tag("tr").body([&]() {
              sender->tag("td").body([&]() { sender->send(entry->index); });
              sender->tag("td").body(
                  [&]() { sender->sendTimestamp(entry->timestamp); });
              sender->tag("td").body(
                  Supla::Device::SecurityLogger::getSourceName(entry->source));
              sender->tag("td").body(entry->log);
            });
          }

          if (count == 0) {
            sender->tag("tr").body(
                [&]() { sender->tag("td").attr("colspan", 4).body("Empty"); });
          }
        });
      });
    });
  });
}

#endif  // ARDUINO_ARCH_AVR
