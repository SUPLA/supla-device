// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "general_purpose_measurement.h"

#include <supla/sensor/measurement_driver.h>
#include <string.h>

using Supla::Sensor::GeneralPurposeMeasurement;

GeneralPurposeMeasurement::GeneralPurposeMeasurement(
    Supla::Sensor::MeasurementDriver *driver,
    bool addMemoryVariableDriver)
    : GeneralPurposeChannelBase(driver, addMemoryVariableDriver) {
  channel.setType(SUPLA_CHANNELTYPE_GENERAL_PURPOSE_MEASUREMENT);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_GENERAL_PURPOSE_MEASUREMENT);
}

