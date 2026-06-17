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
#include <supla/suplet/channel_allocator.h>

using Supla::Suplet::ChannelAllocator;
using Supla::Suplet::ChannelMap;
using Supla::Suplet::kInvalidChannelNumber;

TEST(SupletChannelMapTests, RejectsInvalidAndDuplicateMappings) {
  ChannelMap map;

  EXPECT_FALSE(map.add(0, 1));
  EXPECT_FALSE(map.add(10, -1));
  EXPECT_FALSE(map.add(10, SUPLA_CHANNELMAXCOUNT));

  EXPECT_TRUE(map.add(10, 1));
  EXPECT_FALSE(map.add(10, 2));
  EXPECT_FALSE(map.add(11, 1));

  EXPECT_EQ(map.getCount(), 1);
  EXPECT_EQ(map.getChannelNumber(10), 1);
  EXPECT_EQ(map.getChannelNumber(11), kInvalidChannelNumber);
}

TEST(SupletChannelMapTests, RemoveKeepsRemainingMappings) {
  ChannelMap map;
  ASSERT_TRUE(map.add(10, 1));
  ASSERT_TRUE(map.add(20, 5));
  ASSERT_TRUE(map.add(30, 8));

  EXPECT_TRUE(map.remove(20));
  EXPECT_FALSE(map.remove(20));

  EXPECT_EQ(map.getCount(), 2);
  EXPECT_EQ(map.getChannelNumber(10), 1);
  EXPECT_EQ(map.getChannelNumber(30), 8);
  EXPECT_FALSE(map.containsChannelNumber(5));
}

TEST(SupletChannelAllocatorTests, PreservesExistingMappings) {
  ChannelMap map;
  ASSERT_TRUE(map.add(100, 7));
  ChannelAllocator allocator;
  uint32_t required[] = {100};

  EXPECT_TRUE(allocator.allocateMissing(&map, required, 1));

  EXPECT_EQ(map.getCount(), 1);
  EXPECT_EQ(map.getChannelNumber(100), 7);
}

TEST(SupletChannelAllocatorTests, AllocatesFirstFreeGapsForNewSuplet) {
  ChannelMap map;
  ChannelAllocator allocator;
  ASSERT_TRUE(allocator.markOccupied(0));
  ASSERT_TRUE(allocator.markOccupied(1));
  ASSERT_TRUE(allocator.markOccupied(2));
  ASSERT_TRUE(allocator.markOccupied(3));
  ASSERT_TRUE(allocator.markOccupied(5));
  ASSERT_TRUE(allocator.markOccupied(6));
  ASSERT_TRUE(allocator.markOccupied(7));
  uint32_t required[] = {100, 101, 102, 103};

  EXPECT_TRUE(allocator.allocateMissing(&map, required, 4));

  EXPECT_EQ(map.getChannelNumber(100), 4);
  EXPECT_EQ(map.getChannelNumber(101), 8);
  EXPECT_EQ(map.getChannelNumber(102), 9);
  EXPECT_EQ(map.getChannelNumber(103), 10);
}

TEST(SupletChannelAllocatorTests, KeepsOldAndAllocatesOnlyNewChannels) {
  ChannelMap map;
  ASSERT_TRUE(map.add(100, 12));
  ASSERT_TRUE(map.add(101, 13));
  ChannelAllocator allocator;
  ASSERT_TRUE(allocator.markOccupied(0));
  ASSERT_TRUE(allocator.markOccupied(1));
  uint32_t required[] = {100, 101, 102};

  EXPECT_TRUE(allocator.allocateMissing(&map, required, 3));

  EXPECT_EQ(map.getChannelNumber(100), 12);
  EXPECT_EQ(map.getChannelNumber(101), 13);
  EXPECT_EQ(map.getChannelNumber(102), 2);
}

TEST(SupletChannelAllocatorTests, DoesNotModifyMapWhenAllocationFails) {
  ChannelMap map;
  ASSERT_TRUE(map.add(100, 1));
  ChannelAllocator allocator;
  for (int i = 0; i < SUPLA_CHANNELMAXCOUNT; i++) {
    ASSERT_TRUE(allocator.markOccupied(i));
  }
  uint32_t required[] = {100, 101};

  EXPECT_FALSE(allocator.allocateMissing(&map, required, 2));

  EXPECT_EQ(map.getCount(), 1);
  EXPECT_EQ(map.getChannelNumber(100), 1);
  EXPECT_EQ(map.getChannelNumber(101), kInvalidChannelNumber);
}

TEST(SupletChannelAllocatorTests, RejectsDuplicateRequiredKeysAtomically) {
  ChannelMap map;
  ChannelAllocator allocator;
  uint32_t required[] = {100, 100};

  EXPECT_FALSE(allocator.allocateMissing(&map, required, 2));
  EXPECT_EQ(map.getCount(), 0);
}
