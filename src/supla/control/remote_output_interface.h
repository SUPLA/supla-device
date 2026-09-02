// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_REMOTE_OUTPUT_INTERFACE_H_
#define SRC_SUPLA_CONTROL_REMOTE_OUTPUT_INTERFACE_H_

#include <supla/control/output_interface.h>

namespace Supla {
namespace Control {

/**
 * @brief RemoteOutputInterface class is used as an interface for remote
 * HVAC devices over which we don't have explicit control.
 * When used, HvacBase will not set output value to the device.
 * It allows to set output value from remote device via setOutputValueFromRemote
 * method and it's state will be shared to Supla.
 */
class RemoteOutputInterface : public Supla::Control::OutputInterface {
 public:
   /**
    * Contructor
    *
    * @param onOffOnly if true, output value will be 0 or 1. If false,
    * output value will be 0-100 %
    */
  explicit RemoteOutputInterface(bool onOffOnly);
  ~RemoteOutputInterface() override;

  /**
   * @brief Set output value from a remote device.
   * Use this method to set output value received from a remote device.
   *
   * @param value 0 - off, 1 - on (for on/off only), or 1-100 %
   */
  void setOutputValueFromRemote(int value);

  /**
   * Returns current output value
   *
   * @return 0 - off, 1 - on (for on/off only), or 1-100 %
   */
  int getOutputValue() const override;

  /**
   * Override base class method in order to disable setting output value
   * by HvacBase
   *
   * @param value
   */
  void setOutputValue(int value) override;

  /**
   * Returns if output is on/off only or 0-100%
   *
   * @return true if output is on/off only
   */
  bool isOnOffOnly() const override;

  /**
   * Returns false - output is not controlled internally
   *
   * @return false
   */
  bool isControlledInternally() const override;

 private:
  int outputValue = 0;
  bool onOffOnly = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_REMOTE_OUTPUT_INTERFACE_H_
