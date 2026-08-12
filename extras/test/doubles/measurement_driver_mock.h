// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_MEASUREMENT_DRIVER_MOCK_H_
#define EXTRAS_TEST_DOUBLES_MEASUREMENT_DRIVER_MOCK_H_

#include <gmock/gmock.h>
#include <supla/sensor/measurement_driver.h>

class MeasurementDriverMock : public Supla::Sensor::MeasurementDriver {
 public:
  MOCK_METHOD(void, initialize, (), (override));
  MOCK_METHOD(double, getValue, (), (override));
  MOCK_METHOD(void, setValue, (const double &), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_MEASUREMENT_DRIVER_MOCK_H_
