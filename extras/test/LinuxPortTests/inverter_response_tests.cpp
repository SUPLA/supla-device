// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <supla/pv/afore.h>
#include <supla/pv/fronius.h>
#include <supla/pv/inverter_parser.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {

class AforeForTest : public Supla::PV::Afore {
 public:
  explicit AforeForTest(const char* credentials = "test")
      : Afore(IPAddress("127.0.0.1"), 80, credentials) {
    dataFetchInProgress = true;
  }
  using Afore::loginAndPassword;
  ~AforeForTest() override { delete client; }
  std::array<double, 2> measurements() const {
    return {static_cast<double>(currentPower),
            static_cast<double>(totalGeneratedEnergy)};
  }
};

class FroniusForTest : public Supla::PV::Fronius {
 public:
  explicit FroniusForTest(int type)
      : Fronius(IPAddress("127.0.0.1"), 80, 1, type) {
    dataFetchInProgress = true;
  }
  std::vector<double> measurements() const {
    std::vector<double> values = {
        static_cast<double>(totalGeneratedEnergy),
        static_cast<double>(fwdActEnergy),
        static_cast<double>(rvrActEnergy),
        static_cast<double>(fwdReactEnergy),
        static_cast<double>(rvrReactEnergy),
        static_cast<double>(currentFreq),
        static_cast<double>(invDisabledCounter)};
    for (int phase = 0; phase < 3; phase++) {
      values.push_back(currentCurrent[phase]);
      values.push_back(currentVoltage[phase]);
      values.push_back(currentActivePower[phase]);
      values.push_back(currentApparentPower[phase]);
      values.push_back(currentReactivePower[phase]);
      values.push_back(currentPowerFactor[phase]);
    }
    return values;
  }
  std::string pendingVariable() const {
    return variableToFetch;
  }
};

class InverterResponseTests : public ::testing::Test {
 protected:
  void setupClient() {
    client = new ::testing::NiceMock<NetworkClientMock>();
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
  void consume(Supla::Element* inverter, const std::string& response) {
    input = response;
    position = 0;
    inverter->iterateAlways();
    EXPECT_EQ(position, input.size());
  }
  SimpleTime time;
  NetworkClientMock* client = nullptr;  // Owned by the test inverter.
  std::string input;
  size_t position = 0;
};

TEST_F(InverterResponseTests, AforeBoundsAndTerminatesCredentials) {
  for (size_t length :
       {size_t(0), size_t(3), size_t(99), size_t(100), size_t(120)}) {
    auto* mock = new ::testing::NiceMock<NetworkClientMock>();
    (void)mock;  // Owned by the test inverter.
    std::string input(length, 'x');
    AforeForTest inverter(input.c_str());
    EXPECT_EQ(inverter.loginAndPassword[std::min(
                  length, sizeof(inverter.loginAndPassword) - 1)],
              '\0');
    EXPECT_EQ(std::string(inverter.loginAndPassword),
              input.substr(0, sizeof(inverter.loginAndPassword) - 1));
  }
}

TEST_F(InverterResponseTests, AforeAcceptsCompleteRecordsWithOriginalScales) {
  setupClient();
  AforeForTest inverter;
  consume(&inverter, "var webdata_now_p = \"1.25\";\r\n"
                     "var webdata_total_e = \"12.5\";\n");
  EXPECT_EQ(inverter.measurements(), (std::array<double, 2>{125000, 1250000}));
}

TEST_F(InverterResponseTests, AforeRejectsMalformedRecordsAndRecovers) {
  setupClient();
  AforeForTest inverter;
  consume(&inverter, "var webdata_now_p = \"1\";\n"
                     "var webdata_total_e = \"2\";\n");
  const auto before = inverter.measurements();
  for (const std::string& record : {
           std::string(""), std::string("webdata_now_p ="),
           std::string("webdata_now_p = \"\";"),
           std::string("webdata_now_p = \"3"),
           std::string("webdata_now_p = \"3\""),
           std::string("webdata_now_p = \"3\"; trailing"),
           std::string("webdata_now_p = \"abc\";"),
           std::string("webdata_now_p = \"1.2junk\";"),
           std::string("webdata_now_p = \"nan\";"),
           std::string("webdata_now_p = \"inf\";"),
           std::string("webdata_now_p = \"1e999\";"),
           std::string("webdata_now_p = \"1e-999\";"),
           std::string("webdata_now_p = \"21475\";"),
           std::string("webdata_total_e = \"-1\";"),
           std::string("webdata_total_e = \"1e30\";"),
           "webdata_now_p = \"3" + std::string(100, '0') + "\";"}) {
    SCOPED_TRACE(record);
    consume(&inverter, "var " + record + "\n");
    EXPECT_EQ(inverter.measurements(), before);
  }
  consume(&inverter, "var webdata_now_p = \"-1.5\";\n");
  EXPECT_EQ(inverter.measurements(), (std::array<double, 2>{-150000, 200000}));
}

TEST_F(InverterResponseTests, AforeWaitsForCompletionOfFragmentedLine) {
  setupClient();
  AforeForTest inverter;
  consume(&inverter, "var webdata_now_p = \"1.25");
  EXPECT_EQ(inverter.measurements()[0], 0);
  consume(&inverter, "\";\n");
  EXPECT_EQ(inverter.measurements()[0], 125000);
}

TEST_F(InverterResponseTests, FroniusAcceptsSinglePhaseRecordsAndUnits) {
  setupClient();
  FroniusForTest inverter(0);
  consume(&inverter,
          "\"TOTAL_ENERGY\" : {\n\"Value\" : 12.5,\n}\n"
          "\"PAC\" : {\n\"Unit\" : \"W\",\n\"Value\" : 1.25\n}\n"
          "\"FAC\" : {\n\"Value\" : 50,\n\"Unit\" : \"Hz\"\n}\n"
          "\"IAC\" : {\n\"Value\" : 2.5\n}\n"
          "\"UAC\" : {\n\"Value\" : \"230\"\r\n}\n");
  auto values = inverter.measurements();
  EXPECT_EQ(values[0], 1250);
  EXPECT_EQ(values[5], 5000);
  EXPECT_EQ(values[6], 0);
  EXPECT_EQ(values[7], 2500);
  EXPECT_EQ(values[8], 23000);
  EXPECT_EQ(values[9], 125000);
}

TEST_F(InverterResponseTests, FroniusAcceptsThreePhaseInverterRecords) {
  setupClient();
  FroniusForTest inverter(1);
  consume(&inverter,
          "\"TOTAL_ENERGY\" : {\n\"Value\" : 123\n}\n"
          "\"PAC\" : {\n\"Value\" : 6.5\n}\n"
          "\"IAC_L2\" : {\n\"Value\" : 2.5\n}\n"
          "\"UAC_L3\" : {\n\"Value\" : 231\n}\n");
  auto values = inverter.measurements();
  EXPECT_EQ(values[0], 12300);
  EXPECT_EQ(values[9], 650000);
  EXPECT_EQ(values[13], 2500);
  EXPECT_EQ(values[20], 23100);
}

TEST_F(InverterResponseTests, FroniusAcceptsThreePhaseMeterRecords) {
  setupClient();
  FroniusForTest inverter(2);
  consume(&inverter,
          "\"EnergyReal_WAC_Sum_Consumed\": 12.5,\n"
          "\"EnergyReal_WAC_Sum_Produced\": 10,\n"
          "\"EnergyReactive_VArAC_Sum_Consumed\": 3,\n"
          "\"EnergyReactive_VArAC_Sum_Produced\": 4,\n"
          "\"Frequency_Phase_Average\": 50,\n"
          "\"Current_AC_Phase_2\": 2.5,\n"
          "\"Voltage_AC_Phase_3\": 230,\n"
          "\"PowerApparent_S_Phase_1\": 1.25,\n"
          "\"PowerReal_P_Phase_2\": -1.5,\n"
          "\"PowerReactive_Q_Phase_3\": -2.5,\n"
          "\"PowerFactor_Phase_1\": -0.5\n");
  auto values = inverter.measurements();
  EXPECT_EQ(values[1], 1250);
  EXPECT_EQ(values[2], 1000);
  EXPECT_EQ(values[3], 300);
  EXPECT_EQ(values[4], 400);
  EXPECT_EQ(values[5], 5000);
  EXPECT_EQ(values[10], 125000);
  EXPECT_EQ(values[12], -500);
  EXPECT_EQ(values[13], 2500);
  EXPECT_EQ(values[15], -150000);
  EXPECT_EQ(values[20], 23000);
  EXPECT_EQ(values[23], -250000);
}

TEST_F(InverterResponseTests, FroniusRejectsInvalidValuesAndClearsAssociation) {
  setupClient();
  FroniusForTest inverter(0);
  consume(&inverter, "\"PAC\" : {\n\"Value\" : 1\n}\n");
  const auto before = inverter.measurements();
  for (const auto& value : {"", "\"\"", "abc", "1x", "nan", "inf",
                            "1e999", "1e-999", "21475", "\"3"}) {
    SCOPED_TRACE(value);
    consume(&inverter, "\"PAC\" : {\n\"Value\" : " +
                       std::string(value) + "\n");
    EXPECT_EQ(inverter.measurements(), before);
    EXPECT_TRUE(inverter.pendingVariable().empty());
    consume(&inverter, "\"Value\" : 99\n");
    EXPECT_EQ(inverter.measurements(), before);
  }
  consume(&inverter, "\"PAC\" : {\n\"Value\" : 2\n}\n");
  EXPECT_EQ(inverter.measurements()[9], 200000);
}

TEST_F(InverterResponseTests, FroniusRejectsMalformedAndOversizedRecords) {
  setupClient();
  FroniusForTest inverter(0);
  const auto before = inverter.measurements();
  for (const std::string& record : {
           std::string("\"\""), std::string("\"Value\""),
           std::string("\"Value : 3"), std::string("\"Value\" : 3 extra"),
           "\"Value\" : 3" + std::string(100, ' ')}) {
    SCOPED_TRACE(record);
    consume(&inverter, "\"PAC\" : {\n" + record + "\n");
    EXPECT_EQ(inverter.measurements(), before);
    EXPECT_TRUE(inverter.pendingVariable().empty());
  }
  consume(&inverter, "\"PAC\" : {\n}\n\"Value\" : 3\n");
  EXPECT_EQ(inverter.measurements(), before);
  consume(&inverter, "\"PAC\" : {\n\"broken\" :\n\"Value\" : 3\n");
  EXPECT_EQ(inverter.measurements(), before);
  consume(&inverter, "\"PAC\" : {\nbroken\n\"Value\" : 3\n");
  EXPECT_EQ(inverter.measurements(), before);
  consume(&inverter, "\"PAC\" : {\n\"Value\" : 2\n}\n");
  EXPECT_EQ(inverter.measurements()[9], 200000);
}

TEST_F(InverterResponseTests, FroniusQuotedBraceDoesNotOpenMeasurementObject) {
  setupClient();
  FroniusForTest inverter(0);
  consume(&inverter, "\"PAC\" : {\n\"Value\" : 1\n}\n");
  const auto before = inverter.measurements();
  consume(&inverter, "\"PAC\" : \"{\"\n");
  EXPECT_TRUE(inverter.pendingVariable().empty());
  consume(&inverter, "\"Value\" : 3\n");
  EXPECT_EQ(inverter.measurements(), before);
  // A quoted brace must also clear a previously selected variable.
  consume(&inverter, "\"PAC\" : {\n\"UAC\" : \"{\"\n\"Value\" : 3\n");
  EXPECT_TRUE(inverter.pendingVariable().empty());
  EXPECT_EQ(inverter.measurements(), before);
  consume(&inverter, "\"PAC\" : {\n\"Value\" : \"2\"\n}\n");
  EXPECT_EQ(inverter.measurements()[9], 200000);
}

TEST_F(InverterResponseTests, FroniusUnitObjectClearsMeasurementAssociation) {
  setupClient();
  FroniusForTest inverter(0);
  consume(&inverter, "\"PAC\" : {\n\"Value\" : 1\n}\n");
  const auto before = inverter.measurements();
  consume(&inverter, "\"PAC\" : {\n\"Unit\" : {\n");
  EXPECT_TRUE(inverter.pendingVariable().empty());
  consume(&inverter, "\"Value\" : 3\n}\n}\n");
  EXPECT_EQ(inverter.measurements(), before);
  consume(&inverter, "\"PAC\" : {\n\"Unit\" : \"W\",\n"
                     "\"Value\" : 2\n}\n");
  EXPECT_EQ(inverter.measurements()[9], 200000);
}

TEST_F(InverterResponseTests,
       FroniusNonStringUnitClearsMeasurementAssociation) {
  setupClient();
  FroniusForTest inverter(0);
  consume(&inverter, "\"PAC\" : {\n\"Value\" : 1\n}\n");
  const auto before = inverter.measurements();
  for (const auto& value : {"[", "[]", "{}", "null", "false", "true", "3"}) {
    SCOPED_TRACE(value);
    consume(&inverter, "\"PAC\" : {\n\"Unit\" : " +
                       std::string(value) + "\n");
    EXPECT_TRUE(inverter.pendingVariable().empty());
    consume(&inverter, "{\n\"Value\" : 3\n}\n]\n}\n");
    EXPECT_EQ(inverter.measurements(), before);
  }
  consume(&inverter, "\"PAC\" : {\n\"Unit\" : \"W\",\n"
                     "\"Value\" : 2\n}\n");
  EXPECT_EQ(inverter.measurements()[9], 200000);
}

TEST_F(InverterResponseTests, FroniusRejectsMalformedMeterRecordsAndRecovers) {
  setupClient();
  FroniusForTest inverter(2);
  consume(&inverter, "\"Voltage_AC_Phase_1\" : 230\n");
  const auto before = inverter.measurements();
  for (const auto& value : {"", "\"\"", "abc", "1x", "nan", "inf",
                            "1e999", "1e-999", "-1", "655.36"}) {
    SCOPED_TRACE(value);
    consume(&inverter, "\"Voltage_AC_Phase_1\" : " +
                       std::string(value) + "\n");
    EXPECT_EQ(inverter.measurements(), before);
  }
  consume(&inverter, "\"Voltage_AC_Phase_1\" : 231,\n");
  EXPECT_EQ(inverter.measurements()[8], 23100);
}

TEST_F(InverterResponseTests, FroniusInvalidEnergyAndFrequencyKeepCounters) {
  setupClient();
  FroniusForTest inverter(0);
  consume(&inverter, "\"TOTAL_ENERGY\" : {\n\"Value\" : 12\n}\n");
  const auto before = inverter.measurements();
  consume(&inverter, "\"TOTAL_ENERGY\" : {\n\"Value\" : 1e30\n}\n"
                     "\"FAC\" : {\n\"Value\" : 655.36\n}\n");
  EXPECT_EQ(inverter.measurements(), before);
  EXPECT_EQ(inverter.measurements()[6], 1);
}

TEST_F(InverterResponseTests, FroniusWaitsForCompletionOfFragmentedLine) {
  setupClient();
  FroniusForTest inverter(0);
  consume(&inverter, "\"PAC\" : {\n\"Value\" : 1.");
  EXPECT_EQ(inverter.measurements()[9], 0);
  consume(&inverter, "25\n}\n");
  EXPECT_EQ(inverter.measurements()[9], 125000);
}

TEST(InverterNumericTests, ChecksIntegerBoundsBeforeConversion) {
  uint64_t energy = 42;
  EXPECT_FALSE(Supla::PV::detail::parseInverterValue(
      "18446744073709551616", 1, &energy));
  EXPECT_EQ(energy, 42);
  uint16_t voltage = 42;
  EXPECT_TRUE(Supla::PV::detail::parseInverterValue("655.35", 100, &voltage));
  EXPECT_EQ(voltage, 65535);
  EXPECT_FALSE(Supla::PV::detail::parseInverterValue("655.36", 100, &voltage));
  EXPECT_EQ(voltage, 65535);
  int32_t power = 42;
  EXPECT_TRUE(Supla::PV::detail::parseInverterValue("-2147483648", 1, &power));
  EXPECT_EQ(power, INT32_MIN);
  EXPECT_FALSE(Supla::PV::detail::parseInverterValue("2147483648", 1, &power));
  EXPECT_EQ(power, INT32_MIN);
}

}  // namespace
