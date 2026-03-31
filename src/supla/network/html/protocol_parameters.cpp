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

#ifndef ARDUINO_ARCH_AVR
#include "protocol_parameters.h"

#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

#include "supla/network/network.h"

namespace Supla {

namespace Html {

ProtocolParameters::ProtocolParameters(bool addMqttParams,
                                       bool concurrentProtocols)
    : HtmlElement(HTML_SECTION_PROTOCOL),
      addMqtt(addMqttParams),
      concurrent(concurrentProtocols) {
}

ProtocolParameters::~ProtocolParameters() {
}

void ProtocolParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char buf[512] = {};
    if (!concurrent && addMqtt) {
      sender->tag("div").attr("class", "box").body([&]() {
        sender->formField([&]() {
          sender->labelFor("pro", "Protocol");
          auto select = sender->selectTag("pro", "pro");
          select.attr("onchange", "protocolChanged()").body([&]() {
            sender->selectOption(0, "Supla", cfg->isSuplaCommProtocolEnabled());
            sender->selectOption(1, "MQTT", cfg->isMqttCommProtocolEnabled());
          });
        });
      });
    }

    if (concurrent) {
      sender->tag("div").attr("class", "box").body([&]() {
        sender->tag("h3").body("Supla Settings");
        sender->formField([&]() {
          sender->labelFor("protocol_supla", "Supla protocol");
          auto select = sender->selectTag("protocol_supla", "protocol_supla");
          select.attr("onchange", "protocolChanged()").body([&]() {
            sender->selectOption(
                0, "DISABLED", !cfg->isSuplaCommProtocolEnabled());
            sender->selectOption(
                1, "ENABLED", cfg->isSuplaCommProtocolEnabled());
          });
        });
        sender->toggleBox("proto_supla", true, [&]() {
          // Parameters for Supla protocol
          sender->formField([&]() {
            sender->labelFor("svr", "Server");
            auto input = sender->voidTag("input");
            input.attr("maxlength", 64)
                .attr("placeholder",
                      "Custom server (leave empty for official servers)")
                .attr("name", "svr")
                .attr("id", "svr");
            if (cfg->getSuplaServer(buf)) {
              input.attr("value", buf);
            }
            input.finish();
          });

          sender->formField([&]() {
            sender->labelFor("eml", "E-mail");
            auto input = sender->voidTag("input");
            input.attr("maxlength", 255).attr("name", "eml").attr("id", "eml");
            if (cfg->getEmail(buf)) {
              input.attr("value", buf);
            }
            input.finish();
          });

          sender->send(
              "<script>"
              "function securityChange(){"
              "var e=document.getElementById(\"sec\"),"
              "c=document.getElementById(\"custom_ca_input\"),"
              "l=\"1\"==e.value?\"block\":\"none\";"
              "c.style.display=l;}"
              "</script>");

          uint8_t securityLevel = 0;
          cfg->getUInt8("security_level", &securityLevel);
          sender->formField([&]() {
            sender->labelFor("sec", "Certificate verification");
            auto select = sender->selectTag("sec", "sec");
            select.attr("onchange", "securityChange();").body([&]() {
              sender->selectOption(0, "Supla CA", securityLevel == 0);
              sender->selectOption(1, "Custom CA", securityLevel == 1);
              sender->selectOption(2,
                                   "Skip certificate verification (INSECURE)",
                                   securityLevel == 2);
            });
          });

          sender->toggleBox("custom_ca_input", securityLevel == 1, [&]() {
            sender->formField([&]() {
              sender->labelFor(
                  "custom_ca",
                  "Custom CA (paste here CA certificate in PEM format)");
              auto textarea = sender->tag("textarea");
              textarea.attr("name", "custom_ca")
                  .attr("maxlength", 4000)
                  .close();
              char* bufCert = new char[4000];
              memset(bufCert, 0, 4000);
              if (cfg->getCustomCA(bufCert, 4000)) {
                textarea.body(bufCert);
              } else {
                textarea.body("");
              }
              delete[] bufCert;
            });
          });
        });
      });
    } else {
      sender->tag("div")
          .attr("class", "box")
          .attr("id", "proto_supla")
          .body([&]() {
            sender->tag("h3").body("Supla Settings");
            // Parameters for Supla protocol
            sender->formField([&]() {
              sender->labelFor("svr", "Server");
              auto input = sender->voidTag("input");
              input.attr("maxlength", 64)
                  .attr("placeholder",
                        "Custom server (leave empty for official servers)")
                  .attr("name", "svr")
                  .attr("id", "svr");
              char buf[512];
              if (cfg->getSuplaServer(buf)) {
                input.attr("value", buf);
              }
              input.finish();
            });

            sender->formField([&]() {
              sender->labelFor("eml", "E-mail");
              auto input = sender->voidTag("input");
              input.attr("maxlength", 255)
                  .attr("name", "eml")
                  .attr("id", "eml");
              char buf[512];
              if (cfg->getEmail(buf)) {
                input.attr("value", buf);
              }
              input.finish();
            });

            sender->send(
                "<script>"
                "function securityChange(){"
                "var e=document.getElementById(\"sec\"),"
                "c=document.getElementById(\"custom_ca_input\"),"
                "l=\"1\"==e.value?\"block\":\"none\";"
                "c.style.display=l;}"
                "</script>");

            uint8_t securityLevel = 0;
            cfg->getUInt8("security_level", &securityLevel);
            sender->formField([&]() {
              sender->labelFor("sec", "Certificate verification");
              auto select = sender->selectTag("sec", "sec");
              select.attr("onchange", "securityChange();").body([&]() {
                sender->selectOption(0, "Supla CA", securityLevel == 0);
                sender->selectOption(1, "Custom CA", securityLevel == 1);
                sender->selectOption(2,
                                     "Skip certificate verification (INSECURE)",
                                     securityLevel == 2);
              });
            });

            sender->toggleBox("custom_ca_input", securityLevel == 1, [&]() {
              sender->formField([&]() {
                sender->labelFor(
                    "custom_ca",
                    "Custom CA (paste here CA certificate in PEM format)");
                auto textarea = sender->tag("textarea");
                textarea.attr("name", "custom_ca")
                    .attr("maxlength", 4000)
                    .close();
                char* bufCert = new char[4000];
                memset(bufCert, 0, 4000);
                if (cfg->getCustomCA(bufCert, 4000)) {
                  textarea.body(bufCert);
                } else {
                  textarea.body("");
                }
                delete[] bufCert;
              });
            });
          });
    }

    if (addMqtt) {
      if (concurrent) {
        sender->tag("div").attr("class", "box").body([&]() {
          sender->tag("h3").body("MQTT Settings");
          sender->formField([&]() {
            sender->labelFor("protocol_mqtt", "MQTT protocol");
            auto select = sender->selectTag("protocol_mqtt", "protocol_mqtt");
            select.attr("onchange", "protocolChanged();").body([&]() {
              sender->selectOption(
                  0, "DISABLED", !cfg->isMqttCommProtocolEnabled());
              sender->selectOption(
                  1, "ENABLED", cfg->isMqttCommProtocolEnabled());
            });
          });
          sender->tag("div").attr("class", "mqtt").body([&]() {
            // Parameters for MQTT protocol
            sender->formField([&]() {
              sender->labelFor("mqttserver", "Server");
              auto input = sender->voidTag("input");
              input.attr("maxlength", 64)
                  .attr("name", "mqttserver")
                  .attr("id", "mqttserver");
              if (cfg->getMqttServer(buf)) {
                input.attr("value", buf);
              }
              input.finish();
            });

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

            sender->formField([&]() {
              sender->labelFor("mqtttls", "TLS");
              auto select = sender->selectTag("mqtttls", "mqtttls");
              select.attr("onchange", "mqttTlsChange();").body([&]() {
                sender->selectOption(
                    0, "NO (INSECURE)", !cfg->isMqttTlsEnabled());
                sender->selectOption(1, "YES", cfg->isMqttTlsEnabled());
              });
            });

            sender->numberInput("mqttport",
                                {
                                    .min = 1,
                                    .max = 65535,
                                    .value = cfg->getMqttServerPort(),
                                    .step = 1,
                                });

            sender->formField([&]() {
              sender->labelFor("mqttauth", "Auth");
              auto select = sender->selectTag("mqttauth", "mqttauth");
              select.body([&]() {
                sender->selectOption(0, "NO", !cfg->isMqttAuthEnabled());
                sender->selectOption(1, "YES", cfg->isMqttAuthEnabled());
              });
            });

            sender->formField([&]() {
              sender->labelFor("mqttuser", "Username");
              auto input = sender->voidTag("input");
              input.attr("maxlength", 255)
                  .attr("name", "mqttuser")
                  .attr("id", "mqttuser");
              if (cfg->getMqttUser(buf)) {
                input.attr("value", buf);
              }
              input.finish();
            });

            sender->formField([&]() {
              sender->labelFor("mqttpasswd", "Password (required, max 255)");
              auto input = sender->voidTag("input");
              input.attr("maxlength", 255)
                  .attr("name", "mqttpasswd")
                  .attr("id", "mqttpasswd")
                  .finish();
            });

            sender->formField([&]() {
              sender->labelFor("mqttprefix", "Topic prefix");
              auto input = sender->voidTag("input");
              input.attr("maxlength", 48)
                  .attr("name", "mqttprefix")
                  .attr("id", "mqttprefix");
              if (cfg->getMqttPrefix(buf)) {
                input.attr("value", buf);
              }
              input.finish();
            });

            sender->numberInput("mqttqos",
                                {
                                    .min = 0,
                                    .max = 2,
                                    .value = cfg->getMqttQos(),
                                    .step = 1,
                                });

            sender->formField([&]() {
              sender->labelFor("mqttretain", "Retain");
              auto select = sender->selectTag("mqttretain", "mqttretain");
              select.body([&]() {
                sender->selectOption(0, "NO", !cfg->isMqttRetainEnabled());
                sender->selectOption(1, "YES", cfg->isMqttRetainEnabled());
              });
            });
          });
        });
      } else {
        sender->tag("div").attr("class", "box mqtt").body([&]() {
          sender->tag("h3").body("MQTT Settings");
          // Parameters for MQTT protocol
          sender->formField([&]() {
            sender->labelFor("mqttserver", "Server");
            auto input = sender->voidTag("input");
            input.attr("maxlength", 64)
                .attr("name", "mqttserver")
                .attr("id", "mqttserver");
            if (cfg->getMqttServer(buf)) {
              input.attr("value", buf);
            }
            input.finish();
          });

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

          sender->formField([&]() {
            sender->labelFor("mqtttls", "TLS");
            auto select = sender->selectTag("mqtttls", "mqtttls");
            select.attr("onchange", "mqttTlsChange();").body([&]() {
              sender->selectOption(
                  0, "NO (INSECURE)", !cfg->isMqttTlsEnabled());
              sender->selectOption(1, "YES", cfg->isMqttTlsEnabled());
            });
          });

          sender->numberInput("mqttport",
                              {
                                  .min = 1,
                                  .max = 65535,
                                  .value = cfg->getMqttServerPort(),
                                  .step = 1,
                              });

          sender->formField([&]() {
            sender->labelFor("mqttauth", "Auth");
            auto select = sender->selectTag("mqttauth", "mqttauth");
            select.body([&]() {
              sender->selectOption(0, "NO", !cfg->isMqttAuthEnabled());
              sender->selectOption(1, "YES", cfg->isMqttAuthEnabled());
            });
          });

          sender->formField([&]() {
            sender->labelFor("mqttuser", "Username");
            auto input = sender->voidTag("input");
            input.attr("maxlength", 255)
                .attr("name", "mqttuser")
                .attr("id", "mqttuser");
            if (cfg->getMqttUser(buf)) {
              input.attr("value", buf);
            }
            input.finish();
          });

          sender->formField([&]() {
            sender->labelFor("mqttpasswd", "Password (required, max 255)");
            auto input = sender->voidTag("input");
            input.attr("maxlength", 255)
                .attr("name", "mqttpasswd")
                .attr("id", "mqttpasswd")
                .finish();
          });

          sender->formField([&]() {
            sender->labelFor("mqttprefix", "Topic prefix");
            auto input = sender->voidTag("input");
            input.attr("maxlength", 48)
                .attr("name", "mqttprefix")
                .attr("id", "mqttprefix");
            if (cfg->getMqttPrefix(buf)) {
              input.attr("value", buf);
            }
            input.finish();
          });

          sender->numberInput("mqttqos",
                              {
                                  .min = 0,
                                  .max = 2,
                                  .value = cfg->getMqttQos(),
                                  .step = 1,
                              });

          sender->formField([&]() {
            sender->labelFor("mqttretain", "Retain");
            auto select = sender->selectTag("mqttretain", "mqttretain");
            select.body([&]() {
              sender->selectOption(0, "NO", !cfg->isMqttRetainEnabled());
              sender->selectOption(1, "YES", cfg->isMqttRetainEnabled());
            });
          });
        });
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

#endif  // ARDUINO_ARCH_AVR
