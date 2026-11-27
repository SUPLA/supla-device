// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_ROLLER_SHUTTER_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_ROLLER_SHUTTER_H_

#include <supla/control/roller_shutter.h>
#include <supla/sensor/sensor_parsed.h>

#include <mutex>  // NOLINT(build/c++11)
#include <optional>
#include <string>
#include <tuple>

namespace Supla {
namespace Payload {
class Payload;
}
namespace Control {
class CmdRollerShutter : public Sensor::SensorParsed<RollerShutter> {
 public:
  explicit CmdRollerShutter(Supla::Parser::Parser *parser);

  void relayDownOn() override;
  void relayUpOn() override;
  void relayDownOff() override;
  void relayUpOff() override;

  void iterateAlways() override;
  void onInit() override;
  void onTimer() override;

  void setCmdUpOn(const std::string &);
  void setCmdUpOff(const std::string &);
  void setCmdDownOn(const std::string &);
  void setCmdDownOff(const std::string &);
  void setMotorCommandTimeoutMs(uint32_t timeoutMs);
  void setStateOutput(Payload::Payload *payload, uint32_t intervalMs);

 protected:
  bool isOutputReady() const override;

  std::string cmdUpOn;
  std::string cmdUpOff;
  std::string cmdDownOn;
  std::string cmdDownOff;
  uint32_t lastReadTime = 0;

 private:
  struct State {
    int position;
    int tilt;
    int direction;
    int targetPosition;
    int targetTilt;
    bool tiltEnabled;
    bool calibrated;
    bool calibrationRequested;
    bool calibrationInProgress;
    bool calibrationFailed;
    bool calibrationLost;

    auto fields() const {
      return std::tie(position, tilt, direction, targetPosition, targetTilt,
                      tiltEnabled, calibrated, calibrationRequested,
                      calibrationInProgress, calibrationFailed,
                      calibrationLost);
    }
  };

  void captureState();
  void publishState();
  void runMotorCommand(const std::string &command, const char *context,
                       bool turnOn);

  uint32_t motorCommandTimeoutMs = 1000;
  bool motorCommandFailed = false;
  Payload::Payload *statePayload = nullptr;
  uint32_t outputIntervalMs = 250;
  std::mutex stateMutex;
  std::optional<State> latestState;
  std::optional<State> publishedState;
  std::optional<uint32_t> lastPublishAttempt;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_ROLLER_SHUTTER_H_
