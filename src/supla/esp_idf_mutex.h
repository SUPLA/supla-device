// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_ESP_IDF_MUTEX_H_
#define SRC_SUPLA_ESP_IDF_MUTEX_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <supla/mutex.h>

namespace Supla {

class EspIdfMutex : public Mutex {
 public:
  friend Supla::Mutex *Supla::Mutex::Create();
  virtual ~EspIdfMutex();
  void lock() override;
  void unlock() override;

 protected:
  EspIdfMutex();

  SemaphoreHandle_t mutex = NULL;
};

};  // namespace Supla

#endif  // SRC_SUPLA_ESP_IDF_MUTEX_H_
