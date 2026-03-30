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
