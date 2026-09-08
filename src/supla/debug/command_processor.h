// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEBUG_COMMAND_PROCESSOR_H_
#define SRC_SUPLA_DEBUG_COMMAND_PROCESSOR_H_

#include <stddef.h>
#include <stdint.h>

#if SUPLA_TEST
#include <supla-common/proto.h>
#endif

#include <supla/debug/debug_config.h>

class SuplaDeviceClass;

namespace Supla {
namespace Debug {

class ResponseWriter {
 public:
  virtual ~ResponseWriter() = default;
  virtual void write(const char *text) = 0;
};

class CommandProcessor {
 public:
  explicit CommandProcessor(SuplaDeviceClass *device);

#if SUPLA_TEST
  using TestCalcfgHandler = int (*)(void *context,
                                    TSD_DeviceCalCfgRequest *request,
                                    TDS_DeviceCalCfgResult *result);
  using TestChannelValueHandler = int32_t (*)(
      void *context, TSD_SuplaChannelNewValue *newValue);
  using TestChannelConfigHandler = uint8_t (*)(
      void *context, TSD_ChannelConfig *config, bool local);
  CommandProcessor(SuplaDeviceClass *device,
                   TestCalcfgHandler testCalcfgHandler,
                   void *testCalcfgContext);
  CommandProcessor(SuplaDeviceClass *device,
                   TestChannelValueHandler testChannelValueHandler,
                   void *testChannelValueContext);
  CommandProcessor(SuplaDeviceClass *device,
                   TestChannelConfigHandler testChannelConfigHandler,
                   void *testChannelConfigContext);
#endif

  bool processLine(const char *line, ResponseWriter *writer);

 private:
  struct Command;

  bool parseCommand(const char *json, Command *command);
  void processCommand(const Command &command, ResponseWriter *writer);
  void processDirectCommandJson(const char *line, ResponseWriter *writer);

  void sendError(ResponseWriter *writer, const char *error);
  void sendDone(ResponseWriter *writer, bool ok);
  void sendText(ResponseWriter *writer, const char *text);
  void sendJsonString(ResponseWriter *writer,
                      const char *prefix,
                      const char *value,
                      const char *suffix);

  SuplaDeviceClass *device = nullptr;
#if SUPLA_TEST
  TestCalcfgHandler testCalcfgHandler = nullptr;
  void *testCalcfgContext = nullptr;
  TestChannelValueHandler testChannelValueHandler = nullptr;
  void *testChannelValueContext = nullptr;
  TestChannelConfigHandler testChannelConfigHandler = nullptr;
  void *testChannelConfigContext = nullptr;
#endif
};

}  // namespace Debug
}  // namespace Supla

#endif  // SRC_SUPLA_DEBUG_COMMAND_PROCESSOR_H_
