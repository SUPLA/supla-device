// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_HVAC_REMOTE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_HVAC_REMOTE_H_

#include <supla/control/hvac_base.h>
#include <supla/control/remote_output_interface.h>
#include <supla/sensor/sensor_parsed.h>

#include <optional>
#include <string>

namespace Supla::Output {
class Output;
}
namespace Supla::Parser {
class Parser;
}
namespace Supla::Control {

class HvacRemote : public Sensor::SensorParsed<HvacBase> {
 public:
  struct Fields {
    std::string heating;
    std::string cooling;
    std::string mode;
    std::string setpointHeat;
    std::string setpointCool;
  };

  HvacRemote(Parser::Parser &parser,
             Output::Output &output,
             Fields fields,
             bool heatingIsBinary,
             bool heatCool);
  void iterateAlways() override;

 private:
  struct Settings {
    int mode;
    int heat;
    int cool;
    bool operator==(const Settings &other) const;
  };

  Settings readSettings();
  std::optional<Settings> readDeviceSettings(Settings current) const;
  void readOutputs();
  bool sendSettings(const Settings &settings);
  void importSettings(const Settings &settings);
  void synchronize(bool connected);

  Output::Output &output;
  const Fields fields;
  RemoteOutputInterface heatOutput;
  RemoteOutputInterface coolOutput{true};
  int heatingFeedback = 0;
  std::optional<Settings> synced;
  std::optional<Settings> lastReadback;
  std::optional<uint32_t> lastSendFailure;
};

}  // namespace Supla::Control

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_HVAC_REMOTE_H_
