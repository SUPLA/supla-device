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
"<head>"
"<meta content=\"text/html;charset=UTF-8\"http-equiv=content-type>"
"<meta content=\"width=device-width,initial-scale=1,"
               "maximum-scale=1,user-scalable=no\"name=viewport>"
"<title>Configuration Page</title>";


// CSS from minifier tool.
// Input data is in extras/resources/css_for_cfg_page.css
const char styles[] =
    "<style>"
    ".wrapper,body{min-height:100vh}#logo,body{background:#00d151}.box,button{"
    "box-shadow:0 5px 6px "
    "rgba(0,0,0,.3)}#msg,.box{color:#000}#msg,.slider{top:0;left:0}*,::after,::"
    "before{box-sizing:border-box}html:focus-within{scroll-behavior:smooth}"
    "body,h1,h2,h3,h4,html,p{font-family:HelveticaNeue,\"Helvetica "
    "Neue\",HelveticaNeueRoman,HelveticaNeue-Roman,\"Helvetica Neue "
    "Roman\",TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;"
    "margin:0;padding:0}.box,h1,h3{margin:10px "
    "0}body{text-rendering:optimizeSpeed;line-height:1.5;font-size:14px;font-"
    "weight:400;color:#fff;font-stretch:normal}h1,h3{font-weight:300;font-size:"
    "23px}button,input,select,textarea{font:inherit}.wrapper{display:flex;flex-"
    "direction:column;justify-content:center}.box.collapsible.collapsed "
    ".form-field,.form{display:none}.content{text-align:center;padding:20px "
    "10px}#logo{display:inline-block;height:155px}.box{background:#fff;border-"
    "radius:10px;padding:5px 10px}.box "
    "h3{margin-top:0;margin-bottom:5px}.form{text-align:left;max-width:500px;"
    "margin:-80px auto 0;padding:70px 10px "
    "10px}.form-field{display:flex;align-items:center;padding:8px "
    "10px;border-top:1px solid #00d150;margin:0 "
    "-10px}.box>.form-field:first-of-type{border-top:0}.form-field "
    "label{width:120px;margin-right:5px;color:#00d150}a.wide-link,button{"
    "display:block;color:#fff;font-size:1.3em;text-align:center}@media screen "
    "and (max-width:530px){.form-field{flex-direction:column}.form-field "
    "label{width:100%;margin:3px 0}}.form-field>div{width:100%}.form-field "
    "input:not([type=range]),.form-field select,.form-field "
    "textarea{width:100%;border:1px solid #ccc;border-radius:6px;padding:3px "
    "8px;background:#fff}textarea{resize:vertical}.form-field "
    "select{padding-left:3px}.form-field.checkbox "
    "label{width:100%;color:#000;display:flex;align-items:center;gap:5px}.form-"
    "field.right-checkbox "
    "label:last-child{width:100%;text-align:center}.help-link{cursor:help;"
    "color:#00d150}.box.collapsible "
    "h3,.slider,a.wide-link{cursor:pointer}.hint{font-size:.8em;opacity:.8;"
    "margin-top:2px}button{background:#000;width:100%;border:0;border-radius:"
    "10px;padding:10px;text-transform:uppercase;margin-top:15px}#msg{"
    "background:#ffe836;position:fixed;width:100%;padding:40px;box-shadow:0 "
    "1px 3px "
    "rgba(0,0,0,.3);text-align:center;font-size:26px}a.wide-link{padding:5px;"
    "text-decoration:underline}.box.collapsible "
    "h3:after{content:'↑';float:right}.box.collapsible.collapsed "
    "h3:after{content:'↓'}.switch{position:relative;display:inline-block;width:"
    "51px;height:25px}.switch "
    "input{opacity:0;width:0;height:0}.slider{position:absolute;right:0;bottom:"
    "0;background-color:#ccc;-webkit-transition:.4s;transition:.4s;border-"
    "radius:34px}.slider:before{position:absolute;content:\"\";height:17px;"
    "width:17px;left:4px;bottom:4px;background-color:#fff;-webkit-transition:."
    "4s;transition:.4s;border-radius:50%}input:checked+.slider{background-"
    "color:#00d151}input:focus+.slider{box-shadow:0 0 1px "
    "#00d151}input:checked+.slider:before{-webkit-transform:translateX(26px);-"
    "ms-transform:translateX(26px);transform:translateX(26px)}.range-slider{-"
    "webkit-appearance:none;padding-top:10px;width:100%;height:15px;border-"
    "radius:5px;background:#d3d3d3;outline:0;-webkit-transition:.2s;transition:"
    "opacity "
    ".2s}.range-slider::-webkit-slider-thumb{-webkit-appearance:none;"
    "appearance:none;margin-top:-5px;width:25px;height:25px;border-radius:50%;"
    "background:#00d151;cursor:pointer}.range-slider::-moz-range-thumb{width:"
    "25px;height:25px;border-radius:50%;background:#00d151;cursor:pointer}"
    "</style>";

const char javascript[] =
  "<script>"
  "function showHelp(text){"
    "alert(text);"
    "return false;"
  "}"

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
         "t=document.getElementById(\"mauth_usr\"),"
         "n=document.getElementById(\"mauth_pwd\");"
         "if(e==null){return;}"
         "e=\"1\"==e.value?\"flex\":\"none\";"
         "if(t==null){return;}"
         "if(n==null){return;}"
    "t.style.display=e,n.style.display=e"
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

  "document.getElementById(\"loader\").style.display=\"none\";"
  "document.getElementById(\"form_content\").style.display=\"block\";"

  "</script>";

const char headerEnd[] = "</head>";

const char bodyBegin[] = "<body>";

const char wrapperBegin[] =
  "<div class=\"wrapper\">"
    "<div class=\"content\">";

const char logoSvg[] =
  "<svg id=logo version=1.1 viewBox=\"0 0 200 200\"x=0 xml:space=preserve y=0><"
  "pat"
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

const char dataSavedBox[] = "<div id=\"msg\">Data saved</div>";

Supla::HtmlGenerator::~HtmlGenerator() {}

void Supla::HtmlGenerator::sendPage(Supla::WebSender *sender, bool dataSaved) {
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
  sender->send("</div>");  // .form end
  sender->send("</div>");  // .content end
  sender->send("</div>");  // .wrapper end
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
  sender->send(wrapperBegin, strlen(wrapperBegin));
  sendLogo(sender);
  sender->send("<div id=\"loader\">Loading...</div>");
  sender->send("<div class=\"form\" id=\"form_content\">");
  sendDeviceInfo(sender);
  sendBetaForm(sender);
  sender->send("</div>");  // .form end
  sender->send("</div>");  // .content end
  sender->send("</div>");  // .wrapper end
  sendBodyEnd(sender);
}

void Supla::HtmlGenerator::sendHeaderBegin(Supla::WebSender *sender) {
  sender->send(headerBegin, strlen(headerBegin));
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
  }
}

void Supla::HtmlGenerator::sendForm(Supla::WebSender *sender) {
  sender->send("<form id=\"cfgform\" method=\"post\">");
  sender->send("<div class=\"box\">");
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

  sender->send("<div class=\"box\">");
  sender->send("<h3>Additional Settings</h3>");
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

  sender->send("<div class=\"box\">");
  sender->send("<h3>Additional Settings</h3>");
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
      "<button type=\"button\" onclick=\"saveAndReboot();\">"
        "SAVE &amp; RESTART"
      "</button>"
      "<input type=\"hidden\" name=\"rbt\" value=\"0\" />");
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

