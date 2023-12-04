/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_CLIENT_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_CLIENT_H_

#include <supla/network/client.h>
#include <esp_tls.h>

namespace Supla {
class Mutex;

class EspIdfClient : public Client {
 public:
  EspIdfClient();
  virtual ~EspIdfClient();

  int available() override;
  void stop() override;
  uint8_t connected() override;
  void logConnReason(int, int, int);
  void setTimeoutMs(uint16_t _timeoutMs) override;

 protected:
  int connectImp(const char *host, uint16_t port) override;
  std::size_t writeImp(const uint8_t *buf, std::size_t size) override;
  int readImp(uint8_t *buf, std::size_t size) override;

  Supla::Mutex *mutex = nullptr;
  bool isConnected = false;
  bool firstConnectAfterInit = true;
  esp_tls_t *client = nullptr;
  uint16_t timeoutMs = 10000;
  int lastConnErr = 0;
  int lastTlsErr = 0;
};
};  // namespace Supla


#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_CLIENT_H_
