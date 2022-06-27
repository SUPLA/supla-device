/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

#include "security_certificate.h"
#include "supla/network/network.h"

namespace Supla {

namespace Html {

SecurityCertificate::SecurityCertificate() : HtmlElement(HTML_SECTION_PROTOCOL) {
}

SecurityCertificate::~SecurityCertificate() {
}

void SecurityCertificate::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    sender->send(
  "<script>"
  "function securityChange(){"
  "var e=document.getElementById(\"sec\"),"
  "c=document.getElementById(\"custom_ca\"),"
  "l=\"1\"==e.value?\"block\":\"none\";"
  "c.style.display=l;}"
  "</script>");

    uint8_t securityLevel = 0;
    cfg->getUInt8("security_level", &securityLevel);
    sender->send(
        "<div class=\"w\">"
        "<i><select name=\"sec\" id=\"sec\" onchange=\"securityChange();\">"
        "<option value=\"0\"");
    sender->send(selected(securityLevel == 0));
    sender->send(
        ">Supla CA</option>"
        "<option value=\"1\"");
    sender->send(selected(securityLevel == 1));
    sender->send(
        ">Custom CA</option>"
        "<option value=\"2\"");
    sender->send(selected(securityLevel == 2));
    sender->send(
        ">Skip certificate verification (INSECURE)</option>");

    sender->send(
        "</select>"
        "<label>Certificate verification</label>"
        "</i>");
    sender->send(
        "<i id=\"custom_ca\" style=\"display:");
    if (securityLevel == 1) {
      sender->send("block");
    } else {
      sender->send("none");
    }
    sender->send("\">"
        "<textarea name=\"custom_ca\" maxlength=\"4000\">");
    char *buf = new char[4000];
    if (cfg->getString("custom_ca", buf, 4000)) {
      sender->send(buf);
    }
    delete [] buf;
    sender->send(
        "</textarea><label>Custom CA (paste here CA certificate in PEM format)</label>"
        "</i>"
        "</div>");
  }
}

bool SecurityCertificate::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "sec") == 0) {
    uint8_t securityLevel = stringToUInt(value);
    if (securityLevel > 2) {
      securityLevel = 0;
    }
    cfg->setUInt8("security_level", securityLevel);
    return true;
  } else if (strcmp(key, "custom_ca") == 0) {
    cfg->setString("custom_ca", value);
    return true;
  }

  return false;
}

}  // namespace Html
}  // namespace Supla
