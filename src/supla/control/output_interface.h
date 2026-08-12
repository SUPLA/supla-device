// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_OUTPUT_INTERFACE_H_
#define SRC_SUPLA_CONTROL_OUTPUT_INTERFACE_H_

namespace Supla {

namespace Control {

class OutputInterface {
 public:
  virtual ~OutputInterface() {}
  virtual int getOutputValue() const = 0;
  virtual void setOutputValue(int value) = 0;
  virtual bool isOnOffOnly() const = 0;
  virtual bool isControlledInternally() const { return true; }
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_OUTPUT_INTERFACE_H_
