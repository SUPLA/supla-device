// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_GENERATOR_H_
#define SRC_SUPLA_NETWORK_HTML_GENERATOR_H_

namespace Supla {

enum class SetupRequestResult {
  NONE,
  OK,
  PASSWORD_MISMATCH,
  WEAK_PASSWORD,
  INVALID_REQUEST,
  INVALID_OLD_PASSWORD,
};

class WebSender;

class HtmlGenerator {
 public:
  virtual ~HtmlGenerator();

  virtual void sendPage(Supla::WebSender*,
                        bool dataSaved = false,
                        bool includeSessionLinks = false);
  virtual void sendBetaPage(Supla::WebSender*,
                            bool dataSaved = false,
                            bool includeSessionLinks = false);

  virtual void sendHeaderBegin(Supla::WebSender*);
  virtual void sendTitle(Supla::WebSender*);
  virtual void sendHeader(Supla::WebSender*);
  virtual void sendHeaderEnd(Supla::WebSender*);
  virtual void sendBodyBegin(Supla::WebSender*);
  virtual void sendDataSaved(Supla::WebSender*);
  virtual void sendLogo(Supla::WebSender*);
  virtual void sendDeviceInfo(Supla::WebSender*);
  virtual void sendForm(Supla::WebSender*);  // form send in standard request
  virtual void sendBetaForm(Supla::WebSender*);  // form send in /beta request
  virtual void sendButtons(Supla::WebSender*);
  virtual void sendSubmitButton(Supla::WebSender*);
  virtual void sendBodyEnd(Supla::WebSender*);

  // methods called in sendHeader default implementation
  virtual void sendStyle(Supla::WebSender*);
  virtual void sendJavascript(Supla::WebSender*);

  virtual void sendLoginPage(Supla::WebSender*, bool loginError = false);
  virtual void sendSetupPage(
      Supla::WebSender*,
      bool changePassword,
      Supla::SetupRequestResult = Supla::SetupRequestResult::NONE);
  virtual void sendLogsPage(Supla::WebSender *sender, bool includeSessionLinks);

  virtual void sendSessionLinks(Supla::WebSender*);
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_GENERATOR_H_
