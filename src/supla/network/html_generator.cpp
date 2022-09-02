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

#include "html_generator.h"

#include <string.h>

#include <supla/network/html_element.h>
#include <SuplaDevice.h>

#include "web_sender.h"

const char headerBegin[] =
"<!doctype html>"
"<html lang=en>"
"<meta content=\"text/html;charset=UTF-8\"http-equiv=content-type>"
"<meta content=\"width=device-width,initial-scale=1,"
               "maximum-scale=1,user-scalable=no\"name=viewport>"
"<title>Configuration Page</title>";


const char styles[] =
    "<style>"
    "body{"
      "font-size:14px;"
      "font-family:Helvetica,Tahoma,Geneva,Arial,sans-serif;"
      "background:#00d151;"
      "color:#fff;"
      "line-height:20px;"
      "padding:0"
    "}"
    ".s{"
      "width:580px;"
      "margin:0 auto;"
      "margin-top:calc(50vh - 340px);"
      "border:solid 3px #fff;"
      "padding:0 10px 10px;"
      "border-radius:15px"
    "}"
    "#l{"
      "display:block;"
      "max-width:150px;"
      "height:155px;"
      "margin:-80px auto 20px;"
      "background:#00d151;"
      "padding-right:5px"
    "}"
    "#l path{"
      "fill:#000"
    "}"
    ".w{"
      "margin:3px 0 16px;"
      "padding:5px 0;"
      "border-radius:10px;"
      "background:#fff;"
      "box-shadow:0 5px 6px rgba(0,0,0,.3)"
    "}"
    "h1,h3{"
      "margin:10px 8px;"
      "font-family:Helvetica,Tahoma,Geneva,Arial,sans-serif;"
      "font-weight:300;"
      "color:#000;"
      "font-size:23px"
    "}"
    "h1{"
      "margin-bottom:14px;"
      "color:#fff"
    "}"
    "span{"
      "display:block;"
      "margin:10px 7px 14px"
    "}"
    "div.w span{"
      "color:#000;"
    "}"
    "i{"
      "display:block;"
      "font-style:normal;"
      "position:relative;"
      "border-bottom: solid 1px #00d151;"
      "height:42px"
    "}"
    "i:last-child{"
      "border:none"
    "}"
    "label{"
      "position:absolute;"
      "display:inline-block;"
      "top:5px;"
      "left:8px;"
      "color:#00d151;"
      "pointer-events:none;"
      "font-size:min(14px,3vw);"
      "width:180px"
    "}"
    "input,select,textarea{"
      "width:calc(100% - 183px);"
      "border:none;"
      "font-size:16px;"
      "line-height:40px;"
      "letter-spacing:-.5px;"
      "background:#fff;"
      "color:#000;"
      "padding-left:180px;"
      "-webkit-appearance:none;"
      "-moz-appearance:none;"
      "appearance:none;"
      "outline:0!important;"
      "height:40px"
    "}"
    "select{"
      "padding:0;"
      "float:right;"
      "margin:1px 3px 1px 2px"
    "}"
    "button{"
      "width:100%;"
      "border:0;"
      "background:#000;"
      "padding:5px 10px;"
      "font-size:16px;"
      "line-height:40px;"
      "color:#fff;"
      "border-radius:15px;"
      "box-shadow:0 1px 3px rgba(0,0,0,.3);"
      "cursor:pointer"
    "}"
    ".c{"
      "background:#ffe836;"
      "position:fixed;"
      "width:100%;"
      "line-height:80px;"
      "color:#000;"
      "top:0;"
      "left:0;"
      "box-shadow:0 1px 3px rgba(0,0,0,.3);"
      "text-align:center;"
      "font-size:26px;"
      "z-index:100"
    "}"
    "@media all and (max-height:660px){"
      ".s{"
        "margin-top:80px"
      "}"
    "}"
    "@media all and (max-width:640px){"
      ".s{"
        "width:calc(100% - 20px);"
        "margin-top:40px;"
        "border:none;"
        "padding:0 8px;"
        "border-radius:0"
      "}"
      "#l{"
        "max-width:80px;"
        "height:auto;"
        "margin:10px auto 20px"
      "}"
      "h1,h3{"
        "font-size:19px"
      "}"
      "i{"
        "border:none;"
        "height:auto"
      "}"
      "label{"
        "display:block;"
        "margin:4px 0 12px;"
        "color:#00d151;"
        "font-size:13px;"
        "position:relative;"
        "line-height:18px"
      "}"
      "input,select,textarea{"
        "width:calc(100% - 20px);"
        "font-size:16px;"
        "line-height:28px;"
        "padding:0 5px;"
        "border-bottom:solid 1px #00d151"
      "}"
      "select{"
        "width:100%;"
        "float:none;"
        "margin:0"
      "}"
    "}"
    "#proto_supla{"
      "display:none"
    "}"
    ".proto_mqtt{"
      "display:none"
    "}"
    "textarea{"
      "resize:none;"
      "font-size:12px;"
      "line-height:12px"
    "}"
  "</style>";

const char javascript[] =
  "<script>"

  "function protocolChanged(){"
    "var e=document.getElementById(\"protocol\"),"
         "t=document.getElementById(\"proto_supla\"),"
         "n=document.getElementsByClassName(\"mqtt\"),"
         "l=\"1\"==e.value?\"block\":\"none\";"
    "for(i=0;i<n.length;i++)"
       "n[i].style.display=l;"
    "t.style.display=\"1\"==e.value?\"none\":\"block\""
  "}"

  "function mAuthChanged(){"
    "var e=document.getElementById(\"sel_mauth\"),"
         "t=document.getElementById(\"mauth_usr\"),"
         "n=document.getElementById(\"mauth_pwd\");"
         "e=\"1\"==e.value?\"block\":\"none\";"
    "t.style.display=e,n.style.display=e"
  "}"

  "function saveAndReboot(){"
    "var e=document.getElementById(\"cfgform\");"
    "e.rbt.value=\"2\",e.submit()"
  "}"

  "setTimeout("
    "function(){"
      "var e=document.getElementById(\"msg\");"
      "null!=e&&(e.style.visibility=\"hidden\")"
    "},"
  "3200)"

  "</script>";

const char headerEnd[] = "</head>";

const char bodyBegin[] = "<body onload=protocolChanged(),mAuthChanged()>"
  "<div class=\"s\">";

const char logoSvg[] =
  "<svg id=l version=1.1 viewBox=\"0 0 200 200\"x=0 xml:space=preserve y=0><pat"
  "h d=\"M59.3,2.5c18.1,0.6,31.8,8,40.2,23.5c3.1,5.7,4.3,11.9,4.1,18.3c-0.1,3.6"
  "-0.7,7.1-1.9,10.6c-0.2,0.7-0.1,1.1,0.6,1.5c12.8,7.7,25.5,15.4,38.3,23c2.9,1."
  "7,5.8,3.4,8.7,5.3c1,0.6,1.6,0.6,2.5-0.1c4.5-3.6,9.8-5.3,15.7-5.4c12.5-0.1,22"
  ".9,7.9,25.2,19c1.9,9.2-2.9,19.2-11.8,23.9c-8.4,4.5-16.9,4.5-25.5,0.2c-0.7-0."
  "3-1-0.2-1.5,0.3c-4.8,4.9-9.7,9.8-14.5,14.6c-5.3,5.3-10.6,10.7-15.9,16c-1.8,1"
  ".8-3.6,3.7-5.4,5.4c-0.7,0.6-0.6,1,0,1.6c3.6,3.4,5.8,7.5,6.2,12.2c0.7,7.7-2.2"
  ",14-8.8,18.5c-12.3,8.6-30.3,3.5-35-10.4c-2.8-8.4,0.6-17.7,8.6-22.8c0.9-0.6,1"
  ".1-1,0.8-2c-2-6.2-4.4-12.4-6.6-18.6c-6.3-17.6-12.7-35.1-19-52.7c-0.2-0.7-0.5"
  "-1-1.4-0.9c-12.5,0.7-23.6-2.6-33-10.4c-8-6.6-12.9-15-14.2-25c-1.5-11.5,1.7-2"
  "1.9,9.6-30.7C32.5,8.9,42.2,4.2,53.7,2.7c0.7-0.1,1.5-0.2,2.2-0.2C57,2.4,58.2,"
  "2.5,59.3,2.5z M76.5,81c0,0.1,0.1,0.3,0.1,0.6c1.6,6.3,3.2,12.6,4.7,18.9c4.5,1"
  "7.7,8.9,35.5,13.3,53.2c0.2,0.9,0.6,1.1,1.6,0.9c5.4-1.2,10.7-0.8,15.7,1.6c0.8"
  ",0.4,1.2,0.3,1.7-0.4c11.2-12.9,22.5-25.7,33.4-38.7c0.5-0.6,0.4-1,0-1.6c-5.6-"
  "7.9-6.1-16.1-1.3-24.5c0.5-0.8,0.3-1.1-0.5-1.6c-9.1-4.7-18.1-9.3-27.2-14c-6.8"
  "-3.5-13.5-7-20.3-10.5c-0.7-0.4-1.1-0.3-1.6,0.4c-1.3,1.8-2.7,3.5-4.3,5.1c-4.2"
  ",4.2-9.1,7.4-14.7,9.7C76.9,80.3,76.4,80.3,76.5,81z M89,42.6c0.1-2.5-0.4-5.4-"
  "1.5-8.1C83,23.1,74.2,16.9,61.7,15.8c-10-0.9-18.6,2.4-25.3,9.7c-8.4,9-9.3,22."
  "4-2.2,32.4c6.8,9.6,19.1,14.2,31.4,11.9C79.2,67.1,89,55.9,89,42.6z M102.1,188"
  ".6c0.6,0.1,1.5-0.1,2.4-0.2c9.5-1.4,15.3-10.9,11.6-19.2c-2.6-5.9-9.4-9.6-16.8"
  "-8.6c-8.3,1.2-14.1,8.9-12.4,16.6C88.2,183.9,94.4,188.6,102.1,188.6z M167.7,8"
  "8.5c-1,0-2.1,0.1-3.1,0.3c-9,1.7-14.2,10.6-10.8,18.6c2.9,6.8,11.4,10.3,19,7.8"
  "c7.1-2.3,11.1-9.1,9.6-15.9C180.9,93,174.8,88.5,167.7,88.5z\"/></svg>";

const char bodyEnd[] = "</div></body></html>";

const char dataSavedBox[] = "<div id=\"msg\" class=\"c\">Data saved</div>";

Supla::HtmlGenerator::~HtmlGenerator() {}

void Supla::HtmlGenerator::sendPage(Supla::WebSender *sender, bool dataSaved) {
  sendHeaderBegin(sender);
  sendHeader(sender);
  sendHeaderEnd(sender);
  sendBodyBegin(sender);
  if (dataSaved) {
    sendDataSaved(sender);
  }
  sendLogo(sender);
  sendDeviceInfo(sender);
  sendForm(sender);
  sendBodyEnd(sender);
}

void Supla::HtmlGenerator::sendBetaPage(Supla::WebSender *sender,
    bool dataSaved) {
  sendHeaderBegin(sender);
  sendHeader(sender);
  sendHeaderEnd(sender);
  sendBodyBegin(sender);
  if (dataSaved) {
    sendDataSaved(sender);
  }
  sendLogo(sender);
  sendDeviceInfo(sender);
  sendBetaForm(sender);
  sendBodyEnd(sender);
}

void Supla::HtmlGenerator::sendHeaderBegin(Supla::WebSender *sender) {
  sender->send(headerBegin, strlen(headerBegin));
}

void Supla::HtmlGenerator::sendHeader(Supla::WebSender *sender) {
  sendStyle(sender);
  sendJavascript(sender);
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
  }
}

void Supla::HtmlGenerator::sendForm(Supla::WebSender *sender) {
  sender->send("<form id=\"cfgform\" method=\"post\">");
  sender->send("<div class=\"w\">");
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
      htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_NETWORK) {
      htmlElement->send(sender);
    }
  }
  sender->send("</div>");

  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
      htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_PROTOCOL) {
      htmlElement->send(sender);
    }
  }

  sender->send(
      "<div class=\"w\">"
      "<h3>Additional Settings</h3>");
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
      htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_FORM) {
      htmlElement->send(sender);
    }
  }
  sender->send("</div>");

  sendSubmitButton(sender);
  sender->send("</form>");
}

void Supla::HtmlGenerator::sendBetaForm(Supla::WebSender *sender) {
  sender->send("<form id=\"cfgform\" method=\"post\">");

  sender->send(
      "<div class=\"w\">"
      "<h3>Additional Settings</h3>");
  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
      htmlElement = htmlElement->next()) {
    if (htmlElement->section == HTML_SECTION_BETA_FORM) {
      htmlElement->send(sender);
    }
  }
  sender->send("</div>");

  sendSubmitButton(sender);
  sender->send("</form>");
}

void Supla::HtmlGenerator::sendSubmitButton(Supla::WebSender* sender) {
  sender->send("<button type=\"submit\">SAVE</button>"
      "<br><br>"
      "<button type=\"button\" onclick=\"saveAndReboot();\">"
        "SAVE &amp; RESTART"
      "</button>"
      "<input type=\"hidden\" name=\"rbt\" value=\"0\" />");
}

void Supla::HtmlGenerator::sendBodyEnd(Supla::WebSender *sender) {
  sender->send(bodyEnd, strlen(bodyEnd));
}

// methods called in sendHeader default implementation
void Supla::HtmlGenerator::sendStyle(Supla::WebSender *sender) {
  sender->send(styles, strlen(styles));
}

void Supla::HtmlGenerator::sendJavascript(Supla::WebSender *sender) {
  sender->send(javascript, strlen(javascript));
}

