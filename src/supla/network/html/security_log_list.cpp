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
    : HtmlElement(HTML_SECTION_LOGS), logger(logger)  {
}

SecurityLogList::~SecurityLogList() {
}

void SecurityLogList::send(Supla::WebSender* sender) {
  sender->send("<div class=\"card\">");
  sender->send("<header>");
  sender->send("<h1>Security log</h1>");
  sender->send("</header>");
  if (logger == nullptr) {
    sender->send("<p>No security log</p>");
    sender->send("</div>");
    return;
  }

  sender->send("<main>");
  sender->send("<table aria-label=\"Security log\">");
  sender->send("<thead>");
  sender->send("<tr>");
  sender->send("<th class=\"col-num\">#</th>");
  sender->send("<th class=\"col-ts\">Timestamp</th>");
  sender->send("<th class=\"col-src\">Source</th>");
  sender->send("<th class=\"col-msg\">Message</th>");
  sender->send("</tr>");
  sender->send("</thead>");
  sender->send("<tbody id=\"log-body\">");

  Supla::SecurityLogEntry *entry = nullptr;
  int count = 0;
  logger->prepareGetLog();
  while ((entry = logger->getLog()) != nullptr) {
    if (entry->isEmpty()) {
      continue;
    }
    count++;
    sender->send("<tr><td>");
    sender->send(entry->index);
    sender->send("</td><td>");
    sender->sendTimestamp(entry->timestamp);
    sender->send("</td><td>");
    sender->send(Supla::Device::SecurityLogger::getSourceName(entry->source));
    sender->send("</td><td>");
    sender->send(entry->log);
    sender->send("</td></tr>");
  }

  if (count == 0) {
    sender->send("<tr>");
    sender->send("<td colspan=\"4\">Empty</td>");
    sender->send("</tr>");
  }

  sender->send("</tbody>");
  sender->send("</table>");
  sender->send("</main>");
  sender->send("</div>");
}
