#include "button_update.h"

#include <supla/network/web_sender.h>

#ifdef ARDUINO_ARCH_ESP32
#include <HTTPUpdateServer.h>
HTTPUpdateServer httpUpdater;
#else
#include <ESP8266HTTPUpdateServer.h>
ESP8266HTTPUpdateServer httpUpdater;
#endif

using Supla::Html::ButtonUpdate;

ButtonUpdate::ButtonUpdate(Supla::EspWebServer* server)
    : HtmlElement(HTML_SECTION_BUTTON_AFTER), server(server) {
  // Automatyczna konfiguracja aktualizacji OTA
  httpUpdater.setup(server->getServerPtr(), "/update");
}

void ButtonUpdate::send(Supla::WebSender* sender) {
  sender->send((
      "<button type=\"button\" onclick=\"window.location.href='/update';\">"
      "UPDATE"
      "</button>"));
}
