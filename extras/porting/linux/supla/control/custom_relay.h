
#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_RELAY_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_RELAY_H_

#include <supla/control/virtual_relay.h>
#include <supla/sensor/sensor_parsed.h>

#include <string>

namespace Supla {
namespace Control {
class CustomRelay : public Sensor::SensorParsed<VirtualRelay> {
 public:
  CustomRelay(Supla::Parser::Parser *parser,
             _supla_int_t functions =
                 (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  void onInit() override;
  void turnOn(_supla_int_t duration = 0) override;
  void turnOff(_supla_int_t duration = 0) override;
  bool isOn() override;
  void iterateAlways() override;
  bool isOffline();  // add override

  void setCmdOn(const std::string &);
  void setCmdOff(const std::string &);
  void setTurnOn(const std::string &);
  void setTurnOff(const std::string &);
  void setUseOfflineOnInvalidState(bool useOfflineOnInvalidState);

 protected:
  std::string cmdOn;
  std::string cmdOff;
  std::string setOn;
  std::string setOff;
  uint32_t lastReadTime = 0;
  bool useOfflineOnInvalidState = false;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_RELAY_H_
