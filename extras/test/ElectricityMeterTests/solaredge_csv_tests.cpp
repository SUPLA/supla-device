// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <supla/pv/solaredge.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {

using Measurements = std::array<double, 18>;

class SolarEdgeForTest : public Supla::PV::SolarEdge {
 public:
  SolarEdgeForTest() : SolarEdge("key", "site", "serial", nullptr) {
    temperature = 42;
    totalGeneratedEnergy = 42;
    currentFreq = 42;
    for (int phase = 0; phase < 3; phase++) {
      currentCurrent[phase] = 42;
      currentVoltage[phase] = 42;
      currentApparentPower[phase] = 42;
      currentActivePower[phase] = 42;
      currentReactivePower[phase] = 42;
    }
    bytesCounter = 0;
    retryCounter = 0;
    dataIsReady = false;
    dataFetchInProgress = true;
    headerFound = true;
    connectionTimeoutMs = 0;
  }

  Measurements measurements() const {
    return {temperature,
            static_cast<double>(totalGeneratedEnergy),
            static_cast<double>(currentFreq),
            static_cast<double>(currentCurrent[0]),
            static_cast<double>(currentCurrent[1]),
            static_cast<double>(currentCurrent[2]),
            static_cast<double>(currentVoltage[0]),
            static_cast<double>(currentVoltage[1]),
            static_cast<double>(currentVoltage[2]),
            static_cast<double>(currentApparentPower[0]),
            static_cast<double>(currentApparentPower[1]),
            static_cast<double>(currentApparentPower[2]),
            static_cast<double>(currentActivePower[0]),
            static_cast<double>(currentActivePower[1]),
            static_cast<double>(currentActivePower[2]),
            static_cast<double>(currentReactivePower[0]),
            static_cast<double>(currentReactivePower[1]),
            static_cast<double>(currentReactivePower[2])};
  }
};

std::vector<std::string> validFields() {
  std::vector<std::string> fields(35);
  for (size_t i = 0; i < fields.size(); i++) {
    fields[i] = std::to_string(i) + ".25";
  }
  fields[0] = "2026-10-10 12:00:00";
  fields[1] = "MPPT";
  return fields;
}

std::string csvRow(const std::vector<std::string>& fields) {
  std::string row;
  for (size_t i = 0; i < fields.size(); i++) {
    if (i != 0) {
      row += ',';
    }
    row += fields[i];
  }
  return row;
}

class SolarEdgeCsvTests : public ::testing::Test {
 protected:
  void SetUp() override {
    client = new ::testing::NiceMock<NetworkClientMock>();
    solarEdge = std::make_unique<SolarEdgeForTest>();
    ON_CALL(*client, connected()).WillByDefault(::testing::Return(1));
    EXPECT_CALL(*client, available()).WillRepeatedly(::testing::Invoke(
        [this]() { return static_cast<int>(input.size() - position); }));
    EXPECT_CALL(*client, readImp(::testing::_, 1))
        .WillRepeatedly(::testing::Invoke([this](uint8_t* data, size_t) {
          if (position == input.size()) {
            return 0;
          }
          *data = input[position++];
          return 1;
        }));
  }

  void consume(const std::string& row) {
    input = row + '\n';
    position = 0;
    solarEdge->iterateAlways();
    EXPECT_EQ(position, input.size());
  }

  const Measurements expected = {2.25, 725, 1325,
                                 11250, 19250, 27250,
                                 1225, 2025, 2825,
                                 1425000, 2225000, 3025000,
                                 1525000, 2325000, 3125000,
                                 1625000, 2425000, 3225000};
  SimpleTime time;
  NetworkClientMock* client = nullptr;  // Owned by SolarEdge.
  std::unique_ptr<SolarEdgeForTest> solarEdge;
  std::string input;
  size_t position = 0;
};

TEST_F(SolarEdgeCsvTests, ParsesAll35ColumnsWithOriginalUnitsAndIndices) {
  consume(csvRow(validFields()));
  EXPECT_EQ(solarEdge->measurements(), expected);
}

TEST_F(SolarEdgeCsvTests, EmptyColumnsDoNotShiftMeasurements) {
  auto fields = validFields();
  fields[3].clear();
  fields[11].clear();
  fields[14].clear();
  fields[21].clear();
  fields[27].clear();
  auto expectedWithEmptyFields = expected;
  expectedWithEmptyFields[3] = 0;
  expectedWithEmptyFields[5] = 0;
  expectedWithEmptyFields[9] = 0;
  consume(csvRow(fields));
  EXPECT_EQ(solarEdge->measurements(), expectedWithEmptyFields);
}

TEST_F(SolarEdgeCsvTests, AcceptsSinglePhaseRowsWithEmptyOptionalPhases) {
  auto fields = validFields();
  for (size_t i = 19; i < fields.size(); i++) {
    fields[i].clear();
  }
  consume(csvRow(fields));
  EXPECT_EQ(solarEdge->measurements(),
            (Measurements{2.25, 725, 1325, 11250, 0, 0, 1225, 0, 0,
                          1425000, 0, 0, 1525000, 0, 0, 1625000, 0, 0}));
}

TEST_F(SolarEdgeCsvTests, AcceptsEmptyFirstColumn) {
  auto fields = validFields();
  fields.front().clear();
  consume(csvRow(fields));
  EXPECT_EQ(solarEdge->measurements(), expected);
}

TEST_F(SolarEdgeCsvTests, AcceptsEmptyLastColumn) {
  auto fields = validFields();
  fields.back().clear();
  consume(csvRow(fields));
  EXPECT_EQ(solarEdge->measurements(), expected);
}

TEST_F(SolarEdgeCsvTests, RejectsOnlySeparatorsWithoutChangingMeasurements) {
  const auto before = solarEdge->measurements();
  consume(std::string(34, ','));
  EXPECT_EQ(solarEdge->measurements(), before);
}

TEST_F(SolarEdgeCsvTests, RejectsTooFewColumnsWithoutPartialUpdate) {
  consume(csvRow(validFields()));
  auto fields = validFields();
  fields[2] = "99";
  fields.pop_back();
  consume(csvRow(fields));
  EXPECT_EQ(solarEdge->measurements(), expected);
}

TEST_F(SolarEdgeCsvTests, RejectsTooManyColumnsWithoutPartialUpdate) {
  consume(csvRow(validFields()));
  auto fields = validFields();
  fields[2] = "99";
  fields.push_back("extra");
  consume(csvRow(fields));
  EXPECT_EQ(solarEdge->measurements(), expected);
}

TEST_F(SolarEdgeCsvTests, RejectsNonMpptRowsWithoutChangingMeasurements) {
  consume(csvRow(validFields()));
  auto fields = validFields();
  fields[1] = "SLEEPING";
  fields[2] = "99";
  consume(csvRow(fields));
  EXPECT_EQ(solarEdge->measurements(), expected);
}

TEST_F(SolarEdgeCsvTests, RejectsEmptyModeWithoutChangingMeasurements) {
  const auto before = solarEdge->measurements();
  auto fields = validFields();
  fields[1].clear();
  consume(csvRow(fields));
  EXPECT_EQ(solarEdge->measurements(), before);
}

}  // namespace
