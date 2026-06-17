/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <gtest/gtest.h>
#include <stdint.h>
#include <supla/device/channel_conflict_resolver.h>

namespace {

class TestResolver : public Supla::Device::ChannelConflictResolver {
 public:
  explicit TestResolver(bool result = false) : result(result) {}

  bool onChannelConflictReport(
      uint8_t *channelReport,
      uint8_t channelReportSize,
      bool hasConfilictInvalidType,
      bool hasConfilictChannelMissingOnServer,
      bool hasConflictChannelMissingOnDevice) override {
    calls++;
    lastReport = channelReport;
    lastReportSize = channelReportSize;
    lastInvalidType = hasConfilictInvalidType;
    lastMissingOnServer = hasConfilictChannelMissingOnServer;
    lastMissingOnDevice = hasConflictChannelMissingOnDevice;
    if (order != nullptr && orderSize != nullptr && *orderSize < 4) {
      order[*orderSize] = id;
      (*orderSize)++;
    }
    return result;
  }

  bool result = false;
  int calls = 0;
  int id = 0;
  int *order = nullptr;
  int *orderSize = nullptr;
  uint8_t *lastReport = nullptr;
  uint8_t lastReportSize = 0;
  bool lastInvalidType = false;
  bool lastMissingOnServer = false;
  bool lastMissingOnDevice = false;
};

}  // namespace

TEST(ChannelConflictResolverListTests, EmptyListDoesNotHandleReport) {
  Supla::Device::ChannelConflictResolverList list;
  uint8_t report[] = {0, 1};

  EXPECT_TRUE(list.isEmpty());
  EXPECT_FALSE(list.onChannelConflictReport(report, sizeof(report), true, true,
                                           true));
}

TEST(ChannelConflictResolverListTests, CallsAllResolversInRegistrationOrder) {
  Supla::Device::ChannelConflictResolverList list;
  TestResolver first;
  TestResolver second;
  int order[4] = {};
  int orderSize = 0;
  first.id = 1;
  second.id = 2;
  first.order = order;
  second.order = order;
  first.orderSize = &orderSize;
  second.orderSize = &orderSize;

  EXPECT_TRUE(list.add(&first));
  EXPECT_TRUE(list.add(&second));

  uint8_t report[] = {0, 2, 4};
  EXPECT_FALSE(list.onChannelConflictReport(report, sizeof(report), true, false,
                                           true));

  EXPECT_EQ(first.calls, 1);
  EXPECT_EQ(second.calls, 1);
  EXPECT_EQ(orderSize, 2);
  EXPECT_EQ(order[0], 1);
  EXPECT_EQ(order[1], 2);
  EXPECT_EQ(first.lastReport, report);
  EXPECT_EQ(first.lastReportSize, sizeof(report));
  EXPECT_TRUE(first.lastInvalidType);
  EXPECT_FALSE(first.lastMissingOnServer);
  EXPECT_TRUE(first.lastMissingOnDevice);
}

TEST(ChannelConflictResolverListTests, AggregatesResolverResult) {
  Supla::Device::ChannelConflictResolverList list;
  TestResolver first(false);
  TestResolver second(true);
  TestResolver third(false);

  EXPECT_TRUE(list.add(&first));
  EXPECT_TRUE(list.add(&second));
  EXPECT_TRUE(list.add(&third));

  EXPECT_TRUE(list.onChannelConflictReport(nullptr, 0, false, true, false));
  EXPECT_EQ(first.calls, 1);
  EXPECT_EQ(second.calls, 1);
  EXPECT_EQ(third.calls, 1);
}

TEST(ChannelConflictResolverListTests, IgnoresDuplicatesAndNulls) {
  Supla::Device::ChannelConflictResolverList list;
  TestResolver resolver;

  EXPECT_FALSE(list.add(nullptr));
  EXPECT_TRUE(list.add(&resolver));
  EXPECT_FALSE(list.add(&resolver));

  EXPECT_FALSE(list.onChannelConflictReport(nullptr, 0, false, false, false));
  EXPECT_EQ(resolver.calls, 1);
}

TEST(ChannelConflictResolverListTests, RemoveDetachesResolver) {
  Supla::Device::ChannelConflictResolverList list;
  TestResolver first;
  TestResolver second;

  EXPECT_TRUE(list.add(&first));
  EXPECT_TRUE(list.add(&second));
  EXPECT_TRUE(list.remove(&first));
  EXPECT_FALSE(list.remove(&first));

  EXPECT_FALSE(list.onChannelConflictReport(nullptr, 0, false, false, false));
  EXPECT_EQ(first.calls, 0);
  EXPECT_EQ(second.calls, 1);
  EXPECT_FALSE(list.isEmpty());

  EXPECT_TRUE(list.remove(&second));
  EXPECT_TRUE(list.isEmpty());
}
