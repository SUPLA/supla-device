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

#include "protocol_parameters.h"
#include "supla/network/network.h"

namespace Supla {

namespace Html {

ProtocolParameters::ProtocolParameters(bool addMqttParams,
                                       bool concurrentProtocols)
    : HtmlElement(HTML_SECTION_PROTOCOL), addMqtt(addMqttParams),
      concurrent(concurrentProtocols) {
}

ProtocolParameters::~ProtocolParameters() {
}

void ProtocolParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    if (!concurrent) {
      // Protocol selector
      sender->send("<div class=\"box\">");

      // form-field BEGIN
      sender->send("<div class=\"form-field\">");
      const char key[] = "pro";
      sender->sendLabelFor(key, "Protocol");
      sender->send(
          "<div><select ");
      sender->sendNameAndId(key);
      sender->send(" onchange=\"protocolChanged();\">"
          "<option value=\"0\"");
      sender->send(selected(cfg->isSuplaCommProtocolEnabled()));
      sender->send(
          ">Supla</option>"
          "<option value=\"1\"");
      sender->send(selected(cfg->isMqttCommProtocolEnabled()));
      sender->send(
          ">MQTT</option>"
          "</select></div>");
      sender->send("</div>");
      // form-field END

      sender->send("</div>");  // close box
    }

    if (concurrent) {
      sender->send("<div class=\"box\">");
      sender->send("<h3>Supla Settings</h3>");

      // form-field BEGIN
      sender->send("<div class=\"form-field\">");
      const char keySupla[] = "protocol_supla";
      sender->sendLabelFor(keySupla, "Supla protocol");
      sender->send("<div>");
      sender->send(
          "<select ");
      sender->sendNameAndId(keySupla);
      sender->send(
          "onchange=\"protocolChanged();\">"
          "<option value=\"0\"");
      sender->send(selected(!cfg->isSuplaCommProtocolEnabled()));
      sender->send(
          ">DISABLED</option>"
          "<option value=\"1\"");
      sender->send(selected(cfg->isSuplaCommProtocolEnabled()));
      sender->send(
          ">ENABLED</option>"
          "</select>");
      sender->send("</div>");
      sender->send("</div>");
      // form-field END

      sender->send(
          "<div id=\"proto_supla\">");
    } else {
      sender->send(
          "<div class=\"box\" id=\"proto_supla\">");
      sender->send(
          "<h3>Supla Settings</h3>");
    }
    // Parameters for Supla protocol
    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    const char keySvr[] = "svr";
    sender->sendLabelFor(keySvr, "Server");
    sender->send("<input ");
    sender->sendNameAndId(keySvr);
    sender->send(" maxlength=\"64\" value=\"");
    char buf[512];
    if (cfg->getSuplaServer(buf)) {
      sender->sendSafe(buf);
    }
    sender->send("\">");
    sender->send("</div>");
    // form-field END

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    const char keyEml[] = "eml";
    sender->sendLabelFor(keyEml, "E-mail");
    sender->send("<input ");
    sender->sendNameAndId(keyEml);
    sender->send(" maxlength=\"255\" value=\"");
    if (cfg->getEmail(buf)) {
      sender->sendSafe(buf);
    }
    sender->send("\">");
    sender->send("</div>");  // form-field
    // form-field END

    sender->send(
        "<script>"
        "function securityChange(){"
        "var e=document.getElementById(\"sec\"),"
        "c=document.getElementById(\"custom_ca_input\"),"
        "l=\"1\"==e.value?\"block\":\"none\";"
        "c.style.display=l;}"
        "</script>");

    // form-field BEGIN
    uint8_t securityLevel = 0;
    cfg->getUInt8("security_level", &securityLevel);
    sender->send("<div class=\"form-field\">");
    const char keySec[] = "sec";
    sender->sendLabelFor(keySec, "Certificate verification");
    sender->send("<div>");
    sender->send(
        "<select ");
    sender->sendNameAndId(keySec);
    sender->send(" onchange=\"securityChange();\">"
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
    sender->send(">Skip certificate verification (INSECURE)</option>");
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");  // form-field
    // form-field END

    sender->send("<div id=\"custom_ca_input\" style=\"display:");
    if (securityLevel == 1) {
      sender->send("block");
    } else {
      sender->send("none");
    }
    sender->send("\">");

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    const char keyCustomCa[] = "custom_ca";
    sender->sendLabelFor(
        keyCustomCa, "Custom CA (paste here CA certificate in PEM format)");
    sender->send("<textarea ");
    sender->sendNameAndId(keyCustomCa);
    sender->send("maxlength=\"4000\">");
    char* bufCert = new char[4000];
    memset(bufCert, 0, 4000);
    if (cfg->getCustomCA(bufCert, 4000)) {
      sender->sendSafe(bufCert);
    }
    delete[] bufCert;
    sender->send("</textarea>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END

    sender->send("</div>");
    if (concurrent) {
      sender->send("</div>");
    }

    if (addMqtt) {
      if (concurrent) {
        sender->send(
            "<div class=\"box\">");
        sender->send(
            "<h3>MQTT Settings</h3>");

        // form-field START
        sender->send("<div class=\"form-field\">");
        const char keyProtocolMqtt[] = "protocol_mqtt";
        sender->sendLabelFor(keyProtocolMqtt, "MQTT protocol");
        sender->send("<div>");
        sender->send("<select ");
        sender->sendNameAndId(keyProtocolMqtt);
        sender->send(
            " onchange=\"protocolChanged();\">"
            "<option value=\"0\"");
        sender->send(selected(!cfg->isMqttCommProtocolEnabled()));
        sender->send(
            ">DISABLED</option>"
            "<option value=\"1\"");
        sender->send(selected(cfg->isMqttCommProtocolEnabled()));
        sender->send(
            ">ENABLED</option>"
            "</select>");
        sender->send("</div>");
        sender->send("</div>");
        // form-field END

        sender->send(
            "<div class=\"mqtt\">");
      } else {
        sender->send(
            "<div class=\"box mqtt\">");
        sender->send(
            "<h3>MQTT Settings</h3>");
      }
      // Parameters for MQTT protocol

      // form-field START
      sender->send("<div class=\"form-field\">");
      const char keyMqttServer[] = "mqttserver";
      sender->sendLabelFor(keyMqttServer, "Server");
      sender->send("<input ");
      sender->sendNameAndId(keyMqttServer);
      sender->send(" maxlength=\"64\" value=\"");
      if (cfg->getMqttServer(buf)) {
        sender->sendSafe(buf);
      }
      sender->send("\">");
      sender->send("</div>");
      // form-field END

      sender->send(
          "<script>"
            "function mqttTlsChange(){"
              "var port=document.getElementById(\"mqttport\"),"
              "mqtt_tls=document.getElementById(\"mqtttls\");"
              "if(mqtt_tls.value==\"0\")"
              "{port.value=1883;}else"
              "{port.value=8883;}"
            "}"
          "</script>");

      // form-field START
      sender->send("<div class=\"form-field\">");
      const char keyMqttTls[] = "mqtttls";
      sender->sendLabelFor(keyMqttTls, "TLS");
      sender->send("<div>");
      sender->send("<select ");
      sender->sendNameAndId(keyMqttTls);
      sender->send(" onchange=\"mqttTlsChange();\">"
          "<option value=\"0\" ");
      sender->send(selected(!cfg->isMqttTlsEnabled()));
      sender->send(
          ">NO</option>"
          "<option value=\"1\" ");
      sender->send(selected(cfg->isMqttTlsEnabled()));
      sender->send(
          ">YES</option></select>");
      sender->send("</div>");
      sender->send("</div>");
      // form-field END

      // form-field START
      sender->send("<div class=\"form-field\">");
      const char keyMqttPort[] = "mqttport";
      sender->sendLabelFor(keyMqttPort, "Port");
      sender->send("<input ");
      sender->sendNameAndId(keyMqttPort);
      sender->send(" min=\"1\" max=\"65535\" type=\"number\" value=\"");
      sender->send(cfg->getMqttServerPort());
      sender->send("\">");
      sender->send("</div>");
      // form-field END

      // form-field START
      sender->send("<div class=\"form-field\">");
      const char keyMqttAuth[] = "mqttauth";
      sender->sendLabelFor(keyMqttAuth, "Auth");
      sender->send("<div>");
      sender->send("<select ");
      sender->sendNameAndId(keyMqttAuth);
      sender->send(
          "onchange=\"mAuthChanged();\">"
          "<option value=\"0\" ");
      sender->send(selected(!cfg->isMqttAuthEnabled()));
      sender->send(
          ">NO</option>"
          "<option value=\"1\" ");
      sender->send(selected(cfg->isMqttAuthEnabled()));
      sender->send(">YES</option></select>");
      sender->send("</div>");
      sender->send("</div>");
      // form-field END

      // form-field START
      sender->send("<div class=\"form-field\" id=\"mauth_usr\">");
      const char keyMqttUser[] = "mqttuser";
      sender->sendLabelFor(keyMqttUser, "Username");
      sender->send("<input ");
      sender->sendNameAndId(keyMqttUser);
      sender->send("maxlength=\"255\" value=\"");
      if (cfg->getMqttUser(buf)) {
        sender->sendSafe(buf);
      }
      sender->send("\">");
      sender->send("</div>");
      // form-field END

      // form-field START
      sender->send("<div class=\"form-field\" id=\"mauth_pwd\">");
      const char keyMqttPasswd[] = "mqttpasswd";
      sender->sendLabelFor(keyMqttPasswd, "Password (required, max 255)");
      sender->send("<input ");
      sender->sendNameAndId(keyMqttPasswd);
      sender->send(" maxlength=\"255\">");
      sender->send("</div>");
      // form-field END

      // form-field START
      sender->send("<div class=\"form-field\">");
      const char keyMqttPrefix[] = "mqttprefix";
      sender->sendLabelFor(keyMqttPrefix, "Topic prefix");
      sender->send("<input ");
      sender->sendNameAndId(keyMqttPrefix);
      sender->send(" maxlength=\"48\" value=\"");
      if (cfg->getMqttPrefix(buf)) {
        sender->sendSafe(buf);
      }
      sender->send("\">");
      sender->send("</div>");
      // form-field END

      // form-field START
      sender->send("<div class=\"form-field\">");
      const char keyMqttQos[] = "mqttqos";
      sender->sendLabelFor(keyMqttQos, "QoS");
      sender->send("<input ");
      sender->sendNameAndId(keyMqttQos);
      sender->send(" min=\"0\" max=\"2\" type=\"number\" "
          "value=\"");
      sender->send(cfg->getMqttQos());
      sender->send("\">");
      sender->send("</div>");
      // form-field END

      // form-field START
      sender->send("<div class=\"form-field\">");
      const char keyMqttRetain[] = "mqttretain";
      sender->sendLabelFor(keyMqttRetain, "Retain");
      sender->send("<div>");
      sender->send("<select ");
      sender->sendNameAndId(keyMqttRetain);
      sender->send(">"
          "<option value=\"0\" ");
      sender->send(selected(!cfg->isMqttRetainEnabled()));
      sender->send(
          ">NO</option>"
          "<option value=\"1\" ");
      sender->send(selected(cfg->isMqttRetainEnabled()));
      sender->send(
          ">YES</option></select>");
      sender->send("</div>");
      sender->send("</div>");
      // form-field END

      sender->send("</div>");
      if (concurrent) {
        sender->send("</div>");
      }
    }
  }
}

bool ProtocolParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "pro") == 0) {
    int protocol = stringToUInt(value);
    switch (protocol) {
      default:
      case 0: {
        cfg->setSuplaCommProtocolEnabled(true);
        cfg->setMqttCommProtocolEnabled(false);
        break;
      }
      case 1: {
        cfg->setSuplaCommProtocolEnabled(false);
        cfg->setMqttCommProtocolEnabled(true);
        break;
      }
    }
    return true;
  } else if (strcmp(key, "protocol_supla") == 0) {
    int enabled = stringToUInt(value);
    cfg->setSuplaCommProtocolEnabled(enabled == 1);
    return true;
  } else if (strcmp(key, "protocol_mqtt") == 0) {
    int enabled = stringToUInt(value);
    cfg->setMqttCommProtocolEnabled(enabled == 1);
    return true;
  } else if (strcmp(key, "svr") == 0) {
    cfg->setSuplaServer(value);
    return true;
  } else if (strcmp(key, "eml") == 0) {
    cfg->setEmail(value);
    return true;
  } else if (strcmp(key, "sec") == 0) {
    uint8_t securityLevel = stringToUInt(value);
    if (securityLevel > 2) {
      securityLevel = 0;
    }
    cfg->setUInt8("security_level", securityLevel);
    return true;
  } else if (strcmp(key, "custom_ca") == 0) {
    cfg->setCustomCA(value);
    return true;
  } else if (strcmp(key, "mqttserver") == 0) {
    cfg->setMqttServer(value);
    return true;
  } else if (strcmp(key, "mqttport") == 0) {
    int port = stringToUInt(value);
    cfg->setMqttServerPort(port);
    return true;
  } else if (strcmp(key, "mqtttls") == 0) {
    int enabled = stringToUInt(value);
    cfg->setMqttTlsEnabled(enabled == 1);
    return true;
  } else if (strcmp(key, "mqttauth") == 0) {
    int enabled = stringToUInt(value);
    cfg->setMqttAuthEnabled(enabled == 1);
    return true;
  } else if (strcmp(key, "mqttuser") == 0) {
    cfg->setMqttUser(value);
    return true;
  } else if (strcmp(key, "mqttpasswd") == 0) {
    if (strlen(value) > 0) {
      cfg->setMqttPassword(value);
    }
    return true;
  } else if (strcmp(key, "mqttprefix") == 0) {
    cfg->setMqttPrefix(value);
    return true;
  } else if (strcmp(key, "mqttqos") == 0) {
    int qos = stringToUInt(value);
    cfg->setMqttQos(qos);
    return true;
  } else if (strcmp(key, "mqttretain") == 0) {
    int enabled = stringToUInt(value);
    cfg->setMqttRetainEnabled(enabled == 1);
    return true;
  }

  return false;
}

};  // namespace Html
};  // namespace Supla
