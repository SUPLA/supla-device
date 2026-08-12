// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_OUTPUT_MOCK_H_
#define EXTRAS_TEST_DOUBLES_OUTPUT_MOCK_H_

#include <gmock/gmock.h>
#include <supla/control/output_interface.h>

class OutputSimulator : public Supla::Control::OutputInterface {
 public:
  OutputSimulator();
  virtual ~OutputSimulator();

  int getOutputValue() const override;
  void setOutputValue(int value) override;
  bool isOnOffOnly() const override;

  virtual void setOutputValueCheck(int value);

  int outputValue = 0;
  bool onOffOnly = true;
};

class OutputSimulatorWithCheck : public OutputSimulator {
 public:
  MOCK_METHOD(void, setOutputValueCheck, (int), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_OUTPUT_MOCK_H_
