// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_SECURITY_LOGGER_H_
#define SRC_SUPLA_DEVICE_SECURITY_LOGGER_H_

#include <stdint.h>
#include <stddef.h>

constexpr size_t SUPLA_SECURITY_LOG_ENTRY_SIZE = 64;
constexpr size_t SUPLA_SECURITY_LOG_TEXT_SIZE =
    (SUPLA_SECURITY_LOG_ENTRY_SIZE - sizeof(uint32_t) * 3);

namespace Supla {
class Mutex;

#pragma pack(push, 1)
struct SecurityLogEntry {
  union {
    uint8_t rawData[SUPLA_SECURITY_LOG_ENTRY_SIZE];
    struct {
      uint32_t index;
      uint32_t timestamp;
      union {
        uint32_t source;
        uint8_t sourceBytes[4];
       };
      char log[SUPLA_SECURITY_LOG_TEXT_SIZE];
    };
  };

  void print() const;
  bool isEmpty() const;
};
#pragma pack(pop)

static_assert(sizeof(SecurityLogEntry) == SUPLA_SECURITY_LOG_ENTRY_SIZE);

enum class SecurityLogSource : uint32_t {
  NONE = 0x00000000,
  LOCAL_DEVICE = 0x00000001,
  REMOTE = 0x00000002,
};


namespace Device {
class SecurityLogger {
 public:
  SecurityLogger();
  virtual ~SecurityLogger();
  void log(uint32_t source, const char *log);

  virtual void init();
  virtual bool isEnabled() const;
  virtual void deleteAll();

  // getLog locks the mutex on first call and releases it when all messages
  // have been read, so make sure you call it in a loop until null is returned
  virtual Supla::SecurityLogEntry *getLog();
  virtual bool prepareGetLog();

  virtual void storeLog(const SecurityLogEntry &entry);

  static const char *getSourceName(uint32_t source);

 protected:
  uint32_t index = 0;
  Supla::Mutex *mutex = nullptr;
  static char buffer[SUPLA_SECURITY_LOG_ENTRY_SIZE];
};
};  // namespace Device
};  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_SECURITY_LOGGER_H_
