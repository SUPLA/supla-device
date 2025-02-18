#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_UPDATE_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_UPDATE_H_

#include <supla/network/html_element.h>
#include <supla/network/esp_web_server.h>

namespace Supla {
namespace Html {

class ButtonUpdate : public HtmlElement {
 private:
  Supla::EspWebServer* server;  // Wskaźnik na serwer

 public:
  ButtonUpdate(Supla::EspWebServer* server);
  void send(Supla::WebSender* sender) override;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_BUTTON_UPDATE_H_
