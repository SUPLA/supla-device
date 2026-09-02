// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "html_generator.h"

#include <SuplaDevice.h>
#include <string.h>
#include <supla/device/register_device.h>
#include <supla/network/html_element.h>
#include <supla/time.h>

#include "web_sender.h"

#ifndef SUPLA_CSRF_LOGIN_SETUP_PROTECTION
// Temporary workaround for missing CSRF protection handling in mobile
// client for login and setup pages.
#define SUPLA_CSRF_LOGIN_SETUP_PROTECTION 0
#endif

const char headerBegin[] =
    "<!doctype html>"
    "<html lang=en>"
    "<head>"
    "<meta content=\"text/html;charset=UTF-8\" http-equiv=content-type>"
    "<meta content=\"width=device-width,initial-scale=1,"
    "maximum-scale=1,user-scalable=no\" name=viewport>"
    "<meta name=\"theme-color\" content=\"#00d151\">";

// Generated from extras/resources/css_for_cfg_page.css.
#include "html_generator_styles.inc"

const char javascript[] =
    "<script>"
    "function protocolChanged(){"
    "var e=document.getElementById(\"pro\"),"
    "t=document.getElementById(\"proto_supla\"),"
    "n=document.getElementsByClassName(\"mqtt\"),"
    "show_supla=true,"
    "show_mqtt=true;"
    "if(e==null){"
    "e=document.getElementById(\"protocol_supla\");"
    "if(e==null){return false;}"
    "show_supla=\"1\"==e.value;"
    "e=document.getElementById(\"protocol_mqtt\");"
    "show_mqtt=\"1\"==e.value;"
    "}else if(e.value==\"1\"){"
    "show_supla=false;"
    "}else{"
    "show_mqtt=false;"
    "}"
    "for(i=0;i<n.length;i++)"
    "n[i].style.display=show_mqtt?\"block\":\"none\";"
    "t.style.display=show_supla?\"block\":\"none\";"
    "}"

    "function mAuthChanged(){"
    "var e=document.getElementById(\"mqttauth\"),"
    "t=document.getElementById(\"mqttuser\"),"
    "n=document.getElementById(\"mqttpasswd\");"
    "if(e==null||t==null||n==null){return;}"
    "e=\"1\"==e.value?\"flex\":\"none\";"
    "t.parentNode.style.display=e,n.parentNode.style.display=e"
    "}"

    "function saveAndReboot(){"
    "var e=document.getElementById(\"cfgform\");"
    "e.rbt.value=\"2\",e.submit()"
    "}"

    "setTimeout(function(){"
    "var e=document.getElementById(\"msg\");"
    "null!=e&&(e.style.visibility=\"hidden\")"
    "},3200);"

    "protocolChanged();"
    "mAuthChanged();"

    "var collapsibleBoxes = document.querySelectorAll('.collapsible h3');"
    "for (var i=0;i<collapsibleBoxes.length;i++){"
    "collapsibleBoxes[i].addEventListener('click',function(event){"
    "this.parentNode.classList.toggle('collapsed');"
    "});"
    "}"

    "function findDeviceInfoSpans(){"
    "var spans=document.getElementsByTagName('span'),"
    "guidLabel='G'+'UID:',"
    "macLabel='M'+'AC:',"
    "result=[];"
    "for(var i=0;i<spans.length;i++){"
    "var text=spans[i].textContent;"
    "if(text.indexOf(guidLabel)!=-1||text.indexOf(macLabel)!=-1){"
    "result.push(spans[i]);"
    "}"
    "}"
    "return result;"
    "}"

    "function setDeviceInfoSensitive(hidden){"
    "var values=document.querySelectorAll('.device-info-value');"
    "for(var i=values.length-1;i>=0;i--){"
    "if(!hidden){"
    "var parent=values[i].parentNode;"
    "while(values[i].firstChild){"
    "parent.insertBefore(values[i].firstChild,values[i]);"
    "}"
    "parent.removeChild(values[i]);"
    "parent.normalize();"
    "}"
    "}"
    "if(!hidden){return;}"
    "var spans=findDeviceInfoSpans(),"
    "guidLabel='G'+'UID: ',"
    "macLabel='M'+'AC:';"
    "for(var i=0;i<spans.length;i++){"
    "var walker=document.createTreeWalker(spans[i],4,null,false),"
    "textNodes=[],node;"
    "while(node=walker.nextNode()){textNodes.push(node);}"
    "for(var j=0;j<textNodes.length;j++){"
    "var text=textNodes[j].nodeValue,"
    "guidPos=text.indexOf(guidLabel),"
    "macPos=text.indexOf(macLabel),"
    "pos=guidPos>=0?guidPos:macPos,"
    "labelLength=guidPos>=0?guidLabel.length:macLabel.length;"
    "if(pos<0){continue;}"
    "var range=document.createRange();"
    "range.setStart(textNodes[j],pos+labelLength);"
    "range.setEnd(textNodes[j],text.length);"
    "var value=document.createElement('span');"
    "value.className='sensitive device-info-value';"
    "range.surroundContents(value);"
    "}"
    "}"
    "}"

    "function initPrivacyToggle(){"
    "var sensitive=document.querySelectorAll('.sensitive'),"
    "deviceInfoSpans=findDeviceInfoSpans();"
    "if(!sensitive.length&&!deviceInfoSpans.length){return;}"
    "var button=document.createElement('button');"
    "button.id='privacy-toggle';"
    "button.type='button';"
    "button.innerHTML='<svg class=\"icon-eye\" viewBox=\"0 0 24 24\" "
    "aria-hidden=\"true\"><path d=\"M2 12s3.5-6 10-6 10 6 10 6-3.5 6-10 "
    "6S2 12 2 12z\"/><circle cx=\"12\" cy=\"12\" r=\"2.5\"/></svg>"
    "<svg class=\"icon-eye-off\" viewBox=\"0 0 24 24\" aria-hidden=\"true\">"
    "<path d=\"M3 3l18 18\"/><path d=\"M10.6 6.2A10.9 10.9 0 0 1 12 6c6.5 "
    "0 10 6 10 6a18 18 0 0 1-3.1 3.8\"/><path d=\"M6.6 6.6C3.7 8.5 2 12 "
    "2 12s3.5 6 10 6a10.6 10.6 0 0 0 4.1-.8\"/><path d=\"M9.9 9.9A3 3 0 0 0 "
    "14.1 14.1\"/></svg>';"
    "button.setAttribute('aria-pressed','false');"
    "button.title='Hide sensitive data';"
    "button.setAttribute('aria-label','Hide sensitive data');"
    "button.addEventListener('click',function(){"
    "var hidden=document.body.classList.toggle('privacy-mode');"
    "setDeviceInfoSensitive(hidden);"
    "button.setAttribute('aria-pressed',hidden?'true':'false');"
    "button.title=hidden?'Show sensitive data':'Hide sensitive data';"
    "button.setAttribute('aria-label',button.title);"
    "});"
    "document.body.appendChild(button);"
    "}"

    "initPrivacyToggle();"

    "document.getElementById(\"loader\").style.display=\"none\";"
    "document.getElementById(\"form_content\").style.display=\"block\";"

    "</script>";

const char headerEnd[] = "</head>";

const char bodyBegin[] = "<body>";

const char wrapperBegin[] =
    "<div class=\"wrapper\">"
    "<div class=\"content\">";

const char logoSvg[] =
    "<svg id=logo version=1.1 viewBox=\"0 0 200 200\" x=0 xml:space=preserve "
    "y=0>"
    "<path "
    "d=\"M59.3,2.5c18.1,0.6,31.8,8,40.2,23.5c3.1,5.7,4.3,11.9,4.1,18.3c-0.1,3.6"
    "-0.7,7.1-1.9,10.6c-0.2,0.7-0.1,1.1,0.6,1.5c12.8,7.7,25.5,15.4,38.3,23c2.9,"
    "1."
    "7,5.8,3.4,8.7,5.3c1,0.6,1.6,0.6,2.5-0.1c4.5-3.6,9.8-5.3,15.7-5.4c12.5-0.1,"
    "22"
    ".9,7.9,25.2,19c1.9,9.2-2.9,19.2-11.8,23.9c-8.4,4.5-16.9,4.5-25.5,0.2c-0.7-"
    "0."
    "3-1-0.2-1.5,0.3c-4.8,4.9-9.7,9.8-14.5,14.6c-5.3,5.3-10.6,10.7-15.9,16c-1."
    "8,1"
    ".8-3.6,3.7-5.4,5.4c-0.7,0.6-0.6,1,0,1.6c3.6,3.4,5.8,7.5,6.2,12.2c0.7,7.7-"
    "2.2"
    ",14-8.8,18.5c-12.3,8.6-30.3,3.5-35-10.4c-2.8-8.4,0.6-17.7,8.6-22.8c0.9-0."
    "6,1"
    ".1-1,0.8-2c-2-6.2-4.4-12.4-6.6-18.6c-6.3-17.6-12.7-35.1-19-52.7c-0.2-0.7-"
    "0.5"
    "-1-1.4-0.9c-12.5,0.7-23.6-2.6-33-10.4c-8-6.6-12.9-15-14.2-25c-1.5-11.5,1."
    "7-2"
    "1.9,9.6-30.7C32.5,8.9,42.2,4.2,53.7,2.7c0.7-0.1,1.5-0.2,2.2-0.2C57,2.4,58."
    "2,"
    "2.5,59.3,2.5z "
    "M76.5,81c0,0.1,0.1,0.3,0.1,0.6c1.6,6.3,3.2,12.6,4.7,18.9c4.5,1"
    "7.7,8.9,35.5,13.3,53.2c0.2,0.9,0.6,1.1,1.6,0.9c5.4-1.2,10.7-0.8,15.7,1."
    "6c0.8"
    ",0.4,1.2,0.3,1.7-0.4c11.2-12.9,22.5-25.7,33.4-38.7c0.5-0.6,0.4-1,0-1.6c-5."
    "6-"
    "7.9-6.1-16.1-1.3-24.5c0.5-0.8,0.3-1.1-0.5-1.6c-9.1-4.7-18.1-9.3-27.2-14c-"
    "6.8"
    "-3.5-13.5-7-20.3-10.5c-0.7-0.4-1.1-0.3-1.6,0.4c-1.3,1.8-2.7,3.5-4.3,5.1c-"
    "4.2"
    ",4.2-9.1,7.4-14.7,9.7C76.9,80.3,76.4,80.3,76.5,81z "
    "M89,42.6c0.1-2.5-0.4-5.4-"
    "1.5-8.1C83,23.1,74.2,16.9,61.7,15.8c-10-0.9-18.6,2.4-25.3,9.7c-8.4,9-9.3,"
    "22."
    "4-2.2,32.4c6.8,9.6,19.1,14.2,31.4,11.9C79.2,67.1,89,55.9,89,42.6z "
    "M102.1,188"
    ".6c0.6,0.1,1.5-0.1,2.4-0.2c9.5-1.4,15.3-10.9,11.6-19.2c-2.6-5.9-9.4-9.6-"
    "16.8"
    "-8.6c-8.3,1.2-14.1,8.9-12.4,16.6C88.2,183.9,94.4,188.6,102.1,188.6z "
    "M167.7,8"
    "8.5c-1,0-2.1,0.1-3.1,0.3c-9,1.7-14.2,10.6-10.8,18.6c2.9,6.8,11.4,10.3,19,"
    "7.8"
    "c7.1-2.3,11.1-9.1,9.6-15.9C180.9,93,174.8,88.5,167.7,88.5z\"/></svg>";

const char bodyEnd[] = "</body></html>";

const char dataSavedBox[] = "<div id=\"msg\">Data saved</div>";

Supla::HtmlGenerator::~HtmlGenerator() {
}

void Supla::HtmlGenerator::sendPage(Supla::WebSender *sender,
                                    bool dataSaved,
                                    bool includeSessionLinks) {
  sendHeaderBegin(sender);
  sendHeader(sender);
  sendHeaderEnd(sender);
  sendBodyBegin(sender);
  if (dataSaved) {
    sendDataSaved(sender);
  }
  sender->send(wrapperBegin, strlen(wrapperBegin));
  sendLogo(sender);
  sender->send("<div id=\"loader\">Loading...</div>");
  sender->send("<div class=\"form\" id=\"form_content\">");
  sendDeviceInfo(sender);
  sendForm(sender);
  if (includeSessionLinks) {
    sendSessionLinks(sender);
  }
  sender->send("</div>");  // .form end
  sender->send("</div>");  // .content end
  sender->send("</div>");  // .wrapper end
  sendBodyEnd(sender);
}

void Supla::HtmlGenerator::sendLogsPage(Supla::WebSender *sender,
                                        bool includeSessionLinks) {
  sendHeaderBegin(sender);
  sendHeader(sender);
  sendHeaderEnd(sender);
  sendBodyBegin(sender);
  sender->send(wrapperBegin, strlen(wrapperBegin));
  sendLogo(sender);
  sender->send("<div id=\"loader\">Loading...</div>");
  sender->send("<div id=\"form_content\">");
  sender->send("<div class=\"form\">");
  sendDeviceInfo(sender);
  sender->send("</div>");  // .form end

  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
       htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_LOGS) {
      htmlElement->send(sender);
    }
    delay(0);
  }

  sender->send("<div class=\"form\">");
  sender->send("<a href=\"/\" class=\"wide-link\">Back</a>");
  if (includeSessionLinks) {
    sendSessionLinks(sender);
  }
  sender->send("</div>");  // .form end
  sender->send("</div>");  // .form_content end
  sender->send("</div>");  // .content end
  sender->send("</div>");  // .wrapper end
  sendBodyEnd(sender);
}

void Supla::HtmlGenerator::sendBetaPage(Supla::WebSender *sender,
                                        bool dataSaved,
                                        bool includeSessionLinks) {
  sendHeaderBegin(sender);
  sendHeader(sender);
  sendHeaderEnd(sender);
  sendBodyBegin(sender);
  if (dataSaved) {
    sendDataSaved(sender);
  }
  sender->send(wrapperBegin, strlen(wrapperBegin));
  sendLogo(sender);
  sender->send("<div id=\"loader\">Loading...</div>");
  sender->send("<div class=\"form\" id=\"form_content\">");
  sendDeviceInfo(sender);
  sendBetaForm(sender);
  if (includeSessionLinks) {
    sendSessionLinks(sender);
  }
  sender->send("</div>");  // .form end
  sender->send("</div>");  // .content end
  sender->send("</div>");  // .wrapper end
  sendBodyEnd(sender);
}

void Supla::HtmlGenerator::sendLoginPage(Supla::WebSender *sender,
                                         bool loginError) {
  sendHeaderBegin(sender);
  sendHeader(sender);
  sendHeaderEnd(sender);
  sendBodyBegin(sender);
  sender->send(wrapperBegin, strlen(wrapperBegin));
  sendLogo(sender);
  sender->send("<div id=\"loader\">Loading...</div>");
  sender->send("<div class=\"form\" id=\"form_content\">");
  sender->send("<h1>");
  sender->send(Supla::RegisterDevice::getName());
  sender->send("</h1>");
  if (loginError) {
    sender->send(
        "<div class=\"box\">"
        "<h3>Login failed</h3>"
        "<p>Entered password is incorrect. Please try again.</p>"
        "</div>");
  }
  sender->send("<form method=\"post\">");
  sender->send("<div class=\"box\">");
#if SUPLA_CSRF_LOGIN_SETUP_PROTECTION
  sender->sendCsrfField();
#endif
  sender->send("<div class=\"form-field\">");
  sender->send("<label for=\"cfg_pwd\">Password:</label>");
  sender->send(
      "<input type=\"password\" name=\"cfg_pwd\" id=\"cfg_pwd\" "
      "placeholder=\"Password\" "
      "required>");
  sender->send("</div>");  // .form-field end
  sender->send(
      "<p>Forgot your password? You can change it on Supla Cloud or reset "
      "the device to factory settings.</p>");
  sender->send("<button type=\"submit\">Login</button>");
  sender->send("</div>");  // .box end
  sender->send("</form>");
  sender->send("</div>");  // .form end
  sender->send("</div>");  // .content end
  sender->send("</div>");  // .wrapper end
  sendBodyEnd(sender);
}

void Supla::HtmlGenerator::sendSetupPage(
    Supla::WebSender *sender,
    bool changePassword,
    Supla::SetupRequestResult setupResult) {
  sendHeaderBegin(sender);
  sendHeader(sender);
  sendHeaderEnd(sender);
  sendBodyBegin(sender);
  sender->send(wrapperBegin, strlen(wrapperBegin));
  sendLogo(sender);
  sender->send("<div id=\"loader\">Loading...</div>");
  sender->send("<div class=\"form\" id=\"form_content\">");
  sender->send("<h1>");
  sender->send(Supla::RegisterDevice::getName());
  sender->send("</h1>");
  if (setupResult != Supla::SetupRequestResult::OK &&
      setupResult != Supla::SetupRequestResult::NONE) {
    sender->send(
        "<div class=\"box\">"
        "<h3>Setting new password failed!</h3>");
    switch (setupResult) {
      case Supla::SetupRequestResult::INVALID_OLD_PASSWORD: {
        sender->send("<p>The old password is invalid. Please try again.</p>");
        break;
      }
      case Supla::SetupRequestResult::PASSWORD_MISMATCH: {
        sender->send(
            "<p>The new passwords do not match. Please try again.</p>");
        break;
      }
      case Supla::SetupRequestResult::WEAK_PASSWORD: {
        sender->send("<p>The new password is too weak. Please try again.</p>");
        break;
      }
      case Supla::SetupRequestResult::INVALID_REQUEST: {
        sender->send("<p>Invalid request. Please try again.</p>");
        break;
      }
      default: {
        sender->send("<p>Unknown error. Please try again.</p>");
        break;
      }
    }
    sender->send("</div>");
  }
  sender->send("<form action=\"/setup\" method=\"POST\">");
  sender->send("<div class=\"box\">");
#if SUPLA_CSRF_LOGIN_SETUP_PROTECTION
  sender->sendCsrfField();
#endif
  if (changePassword) {
    sender->send("<h3>Change password</h3>");
    sender->send("<div class=\"form-field\">");
    sender->send("<label for=\"old_cfg_pwd\">Current password:</label>");
    sender->send(
        "<input type=\"password\" name=\"old_cfg_pwd\" id=\"old_cfg_pwd\" "
        "required>");
    sender->send("</div>");  // .form-field end
  } else {
    sender->send("<h3>Set password</h3>");
  }

  sender->send("<div class=\"form-field\">");
  sender->send("<label for=\"cfg_pwd\">New password:</label>");
  sender->send(
      "<input type=\"password\" name=\"cfg_pwd\" id=\"cfg_pwd\" "
      "required>");
  sender->send("</div>");  // .form-field end

  sender->send("<div class=\"form-field\">");
  sender->send("<label for=\"confirm_cfg_pwd\">Confirm password:</label>");
  sender->send(
      "<input type=\"password\" name=\"confirm_cfg_pwd\" "
      "id=\"confirm_cfg_pwd\" required>");
  sender->send("</div>");  // .form-field end
  sender->send(
      "<p>Password should be at least 8 characters "
      "long and contain at least one uppercase [A-Z], one lowercase "
      "letter [a-z], and one digit [0-9].</p>");
  if (changePassword) {
    sender->send("<button type=\"submit\">Change password</button>");
    sender->send("<a href=\"/\" class=\"wide-link\">Cancel</a>");
  } else {
    sender->send("<button type=\"submit\">Create new password</button>");
  }
  sender->send("</div>");  // .box end
  sender->send("</form>");
  sender->send("</div>");  // .form end
  sender->send("</div>");  // .content end
  sender->send("</div>");  // .wrapper end
  sendBodyEnd(sender);
}

void Supla::HtmlGenerator::sendHeaderBegin(Supla::WebSender *sender) {
  sender->send(headerBegin, strlen(headerBegin));
  sendTitle(sender);
}

void Supla::HtmlGenerator::sendTitle(Supla::WebSender *sender) {
  sender->send("<title>");
  sender->sendSafe(Supla::RegisterDevice::getName());
  sender->send("</title>");
}

void Supla::HtmlGenerator::sendHeader(Supla::WebSender *sender) {
  sendStyle(sender);
}

void Supla::HtmlGenerator::sendHeaderEnd(Supla::WebSender *sender) {
  sender->send(headerEnd, strlen(headerEnd));
}

void Supla::HtmlGenerator::sendBodyBegin(Supla::WebSender *sender) {
  sender->send(bodyBegin, strlen(bodyBegin));
}

void Supla::HtmlGenerator::sendDataSaved(Supla::WebSender *sender) {
  sender->send(dataSavedBox);
}

void Supla::HtmlGenerator::sendLogo(Supla::WebSender *sender) {
  sender->send(logoSvg, strlen(logoSvg));
}

void Supla::HtmlGenerator::sendDeviceInfo(Supla::WebSender *sender) {
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
       htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_DEVICE_INFO) {
      htmlElement->send(sender);
    }
    delay(0);
  }
}

void Supla::HtmlGenerator::sendForm(Supla::WebSender *sender) {
  sender->send("<form id=\"cfgform\" method=\"post\">");
  sender->send("<div class=\"box\">");
  sender->sendCsrfField();
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
       htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_NETWORK) {
      htmlElement->send(sender);
    }
    delay(0);
  }
  sender->send("</div>");

  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
       htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_PROTOCOL) {
      htmlElement->send(sender);
    }
    delay(0);
  }

  bool boxSend = false;
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
       htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_FORM) {
      if (!boxSend) {
        sender->send("<div class=\"box\">");
        sender->send("<h3>Additional Settings</h3>");
        boxSend = true;
      }
      htmlElement->send(sender);
    }
    delay(0);
  }
  if (boxSend) {
    sender->send("</div>");
  }

  sendButtons(sender);
  sender->send("</form>");
}

void Supla::HtmlGenerator::sendBetaForm(Supla::WebSender *sender) {
  sender->send("<form id=\"cfgform\" method=\"post\">");
  sender->sendCsrfField();

  bool boxSend = false;
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
       htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_BETA_FORM) {
      if (!boxSend) {
        sender->send("<div class=\"box\">");
        sender->send("<h3>Additional Settings</h3>");
        boxSend = true;
      }
      htmlElement->send(sender);
    }
    delay(0);
  }
  if (boxSend) {
    sender->send("</div>");
  }

  sendButtons(sender);
  sender->send("</form>");
}

void Supla::HtmlGenerator::sendButtons(Supla::WebSender *sender) {
  bool securityLogsEnabled = false;
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
       htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_BUTTON_BEFORE) {
      htmlElement->send(sender);
    }
    if (htmlElement->section == HTML_SECTION_LOGS) {
      securityLogsEnabled = true;
    }
    delay(0);
  }
  sendSubmitButton(sender);
  if (securityLogsEnabled) {
    sender->send("<a href=\"/logs\" class=\"wide-link\">Security logs</a>");
  }
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
       htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_BUTTON_AFTER) {
      htmlElement->send(sender);
    }
    delay(0);
  }
}

void Supla::HtmlGenerator::sendSubmitButton(Supla::WebSender *sender) {
  sender->send(
      "<button type=\"submit\">SAVE</button>"
      "<button type=\"button\" onclick=\"saveAndReboot();\">"
      "SAVE &amp; RESTART"
      "</button>"
      "<input type=\"hidden\" name=\"rbt\" value=\"0\">");
}

void Supla::HtmlGenerator::sendSessionLinks(Supla::WebSender *sender) {
  // logout button
  sender->send(
      "<form method=\"post\" action=\"/logout\" style=\"display: inline;\">");
  sender->sendCsrfField();
  sender->send(
      "<button type=\"submit\">"
      "Logout"
      "</button>"
      "</form>");
  // change password link
  sender->send("<a href=\"/setup\" class=\"wide-link\">Change password</a>");
}

void Supla::HtmlGenerator::sendBodyEnd(Supla::WebSender *sender) {
  sendJavascript(sender);
  sender->send(bodyEnd, strlen(bodyEnd));
}

// methods called in sendHeader default implementation
void Supla::HtmlGenerator::sendStyle(Supla::WebSender *sender) {
  sender->send(styles, strlen(styles));
}

void Supla::HtmlGenerator::sendJavascript(Supla::WebSender *sender) {
  sender->send(javascript, strlen(javascript));
}
