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

#ifndef SRC_SUPLA_NETWORK_WEB_SERVER_H_
#define SRC_SUPLA_NETWORK_WEB_SERVER_H_

#include <supla/network/html_generator.h>
#include <supla/network/html_element.h>
#include <supla/device/security_logger.h>
#include <stdint.h>

class SuplaDeviceClass;

#define HTML_KEY_LENGTH 16
#define HTML_VAL_LENGTH 4000

namespace Supla {

extern const unsigned char favico[1150];

class WebServer {
 public:
  static WebServer *Instance();
  explicit WebServer(Supla::HtmlGenerator *);
  virtual ~WebServer();
  virtual void start() = 0;
  virtual void stop() = 0;
  void setSuplaDeviceClass(SuplaDeviceClass *);
  void notifyClientConnected(bool isPost = false);
  virtual void parsePost(const char *postContent,
                         int size,
                         bool lastChunk = true);
  virtual void resetParser();
  void setBetaProcessing();

  virtual bool verifyCertificatesFormat();

  Supla::HtmlGenerator *htmlGenerator = nullptr;

 protected:
  void addSecurityLog(Supla::SecurityLogSource source, const char *log) const;
  void addSecurityLog(uint32_t source, const char *log) const;
  /**
   * Check if section is allowed in current POST request. It excludes
   * html elements in /beta page POST, and vice versa
   *
   * @param section
   *
   * @return true if section is allowed to be processed
   */
  bool isSectionAllowed(Supla::HtmlSection section) const;

  static WebServer *webServerInstance;
  bool destroyGenerator = false;
  SuplaDeviceClass *sdc = nullptr;
  bool keyFound = false;
  int partialSize = 0;
  char key[HTML_KEY_LENGTH] = {};
  char *value = nullptr;
  bool betaProcessing = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_WEB_SERVER_H_
