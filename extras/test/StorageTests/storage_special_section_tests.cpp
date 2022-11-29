/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/storage/storage.h>
#include <storage_mock.h>
#include <string.h>
#include <stdio.h>

using ::testing::Return;
using ::testing::_;
using ::testing::AtLeast;

TEST(StorageSpecialSecTests, registerSectionTest) {
  StorageMock storage;

  // registration of specific ID will succeed only once. Subsequent calls
  // will fail
  EXPECT_TRUE(Supla::Storage::RegisterSection(1, 10, 20, false, false));
  EXPECT_FALSE(Supla::Storage::RegisterSection(1, 10, 20, false, false));
  EXPECT_TRUE(Supla::Storage::RegisterSection(2, 10, 20, true, true));
  EXPECT_FALSE(Supla::Storage::RegisterSection(1, 10, 20, false, false));
  EXPECT_FALSE(Supla::Storage::RegisterSection(2, 10, 20, false, false));

  // crc without backup is ok
  EXPECT_TRUE(Supla::Storage::RegisterSection(10, 10, 20, true, false));
  // crc with backup is ok
  EXPECT_TRUE(Supla::Storage::RegisterSection(11, 10, 20, true, true));
  // backup without crc is not ok (not possible to check which one is not
  // corrupted
  EXPECT_FALSE(Supla::Storage::RegisterSection(12, 10, 20, false, true));
}

TEST(StorageSpecialSecTests, writeAndReadTest) {
  StorageMock storage;
  char dataSample[] = "SUPLA";
  char buf[100] = {};

  // Write and read fails if section wasn't configured
  EXPECT_FALSE(Supla::Storage::WriteSection(
        13, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_FALSE(Supla::Storage::ReadSection(
        13, reinterpret_cast<unsigned char*>(buf), 6));

  // Register secion 51, try to write and read
  EXPECT_TRUE(Supla::Storage::RegisterSection(51, 10, 6, false, false));

  EXPECT_CALL(storage, writeStorage(10, _, 6))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      });

  // Each write also calls read in order to check if write is required,
  // but then last param is set to false
  EXPECT_CALL(storage, readStorage(10, _, 6, false))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        // read before write operation - data has to be different in order
        // to call write
        snprintf(reinterpret_cast<char*>(data), 6, "supla");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        // read before write operation - data has to be different in order
        // to call write
        snprintf(reinterpret_cast<char*>(data), 6, "supla");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        // read before write operation - data has to be different in order
        // to call write
        snprintf(reinterpret_cast<char*>(data), 6, "supla");
        return 6;
      })
  ;
  EXPECT_CALL(storage, readStorage(10, _, 6, true))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        // normal read
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLa");
        return 6;
      })
  ;

  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_STREQ(buf, "");
  EXPECT_TRUE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
  EXPECT_STREQ(buf, "SUPLA");

  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_TRUE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
  EXPECT_STREQ(buf, "SUPLa");

  // Write and read with invalid sizes should fail without actual read/write
  EXPECT_FALSE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 5));
  EXPECT_FALSE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 5));
  EXPECT_FALSE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 7));
  EXPECT_FALSE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 7));
}

TEST(StorageSpecialSecTests, writeAndReadTestWithCrc) {
  StorageMock storage;
  char dataSample[] = "SUPLA";
  char buf[100] = {};

  // Register secion 51, try to write and read
  EXPECT_TRUE(Supla::Storage::RegisterSection(51, 10, 6, true, false));

  // data writes
  EXPECT_CALL(storage, writeStorage(10, _, 6))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    ;
  // crc writes
  EXPECT_CALL(storage, writeStorage(16, _, 2))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    ;

  // reads executed before write operation
  EXPECT_CALL(storage, readStorage(10, _, 6, false))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    ;

  // read data
  EXPECT_CALL(storage, readStorage(10, _, 6, true))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
    // third read with invalid data vs crc
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alama");
        return 6;
      })
    // fourth read with invalid data vs crc
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
  ;
  // read crc
  EXPECT_CALL(storage, readStorage(16, _, 2, true))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
    // third read with invalid data
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
    // fourth read with invalid crc
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 59615;
        memcpy(data, &crc, 2);
        return 2;
      })
  ;

  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_STREQ(buf, "");
  EXPECT_TRUE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
  EXPECT_STREQ(buf, "SUPLA");

  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_TRUE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
  EXPECT_STREQ(buf, "SUPLA");

  // fourth read with invalid data
  EXPECT_FALSE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));

  // fifth read with invalid crc
  EXPECT_FALSE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
}

TEST(StorageSpecialSecTests, writeAndReadTestWithCrcAndBackup) {
  StorageMock storage;
  char dataSample[] = "SUPLA";
  char buf[100] = {};

  // Register secion 51, try to write and read
  EXPECT_TRUE(Supla::Storage::RegisterSection(51, 10, 6, true, true));

  // data writes
  EXPECT_CALL(storage, writeStorage(10, _, 6))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    ;
  // backup data writes (address 10 + size of section + crc size)
  EXPECT_CALL(storage, writeStorage(10 + 6 + 2, _, 6))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(strncmp(reinterpret_cast<const char*>(data), "SUPLA", 6), 0);
        return 6;
      })
    ;

  // reads executed before write operation
  EXPECT_CALL(storage, readStorage(10, _, 6, false))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    ;
  // reads of backup executed before write operation
  EXPECT_CALL(storage, readStorage(10 + 6 + 2, _, 6, false))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alpus");
        return 6;
      })
    ;

  // crc writes
  EXPECT_CALL(storage, writeStorage(16, _, 2))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    ;
  // backup crc writes (offset + size + crc + size)
  EXPECT_CALL(storage, writeStorage(16 + 2 + 6, _, 2))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t *>(data), 62432);
        return 2;
      })
    ;

  // read data
  EXPECT_CALL(storage, readStorage(10, _, 6, true))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
    // third read with invalid data vs crc
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alama");
        return 6;
      })
    // fourth read with invalid data vs crc
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "oh no");
        return 6;
      })
  ;
  // read data from backup happens only on 3-5 step
  // in steps 1-2 data and crc are valid
  EXPECT_CALL(storage, readStorage(10 + 6 + 2, _, 6, true))
    // third (backup ok)
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
    // fourth (backup ok)
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "SUPLA");
        return 6;
      })
    // fifth (backup nok)
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        snprintf(reinterpret_cast<char*>(data), 6, "alama");
        return 6;
      })
  ;
  // read crc
  EXPECT_CALL(storage, readStorage(16, _, 2, true))
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
    // third read with invalid data
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
    // fourth read with invalid crc
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 59615;
        memcpy(data, &crc, 2);
        return 2;
      })
    // fifth read
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
  ;

  // read crc from backup
  EXPECT_CALL(storage, readStorage(16 + 2 + 6, _, 2, true))
    // ok
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
  // ok
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 62432;
        memcpy(data, &crc, 2);
        return 2;
      })
    // fifth step: invalid crc
    .WillOnce(
        [] (int offset, unsigned char *data, int size, bool logs) {
        uint16_t crc = 1234;
        memcpy(data, &crc, 2);
        return 2;
      })
  ;

  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_STREQ(buf, "");
  EXPECT_TRUE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
  EXPECT_STREQ(buf, "SUPLA");

  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_TRUE(Supla::Storage::WriteSection(
        51, reinterpret_cast<unsigned char*>(dataSample), 6));
  EXPECT_TRUE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
  EXPECT_STREQ(buf, "SUPLA");

  // third read with invalid data, however backup is ok
  EXPECT_TRUE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));

  // fourth read with invalid crc, however backup is ok
  EXPECT_TRUE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
  // fifth read with invalid crc, however backup is ok
  EXPECT_FALSE(Supla::Storage::ReadSection(
        51, reinterpret_cast<unsigned char*>(buf), 6));
}

TEST(StorageSpecialSecTests, deleteSectionTest) {
  StorageMock storage;
  char dataSample[] = "SUPLA";
  char buf[100] = {};

  // invalid section id
  EXPECT_FALSE(Supla::Storage::DeleteSection(13));

  // Register secion 51, try to write and read
  EXPECT_TRUE(Supla::Storage::RegisterSection(51, 10, 6, false, false));

  EXPECT_CALL(storage, writeStorage(10, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(11, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(12, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(13, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(14, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(15, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  /*
  // crc clean
  EXPECT_CALL(storage, writeStorage(16, _, 2))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t*>(data), 0);
        return 2;
      });
      */

  EXPECT_TRUE(Supla::Storage::DeleteSection(51));
}

TEST(StorageSpecialSecTests, deleteSectionTestWithCrc) {
  StorageMock storage;
  char dataSample[] = "SUPLA";
  char buf[100] = {};

  // Register secion 51, try to write and read
  EXPECT_TRUE(Supla::Storage::RegisterSection(51, 10, 6, true, false));

  EXPECT_CALL(storage, writeStorage(10, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(11, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(12, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(13, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(14, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(15, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  // crc clean
  EXPECT_CALL(storage, writeStorage(16, _, 2))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t*>(data), 0);
        return 2;
      });

  EXPECT_TRUE(Supla::Storage::DeleteSection(51));
}

TEST(StorageSpecialSecTests, deleteSectionTestWithCrcAndBackup) {
  StorageMock storage;
  char dataSample[] = "SUPLA";
  char buf[100] = {};

  // Register secion 51, try to write and read
  EXPECT_TRUE(Supla::Storage::RegisterSection(51, 10, 6, true, true));

  EXPECT_CALL(storage, writeStorage(10, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(11, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(12, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(13, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(14, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(15, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  // crc clean
  EXPECT_CALL(storage, writeStorage(16, _, 2))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t*>(data), 0);
        return 2;
      });

  // backup
  EXPECT_CALL(storage, writeStorage(18, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(19, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(20, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(21, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(22, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  EXPECT_CALL(storage, writeStorage(23, _, 1))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*data, 0);
        return 1;
      });
  // crc clean
  EXPECT_CALL(storage, writeStorage(24, _, 2))
    .WillOnce(
        [] (int offset, const unsigned char *data, int size) {
        EXPECT_EQ(*reinterpret_cast<const uint16_t*>(data), 0);
        return 2;
      });

  EXPECT_TRUE(Supla::Storage::DeleteSection(51));
}

