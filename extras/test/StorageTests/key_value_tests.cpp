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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/storage/key_value.h>
#include "supla/storage/config.h"

class KeyValueTest : public Supla::KeyValue {
  public:
    bool init() override {
      return true;
    };
    void removeAll() override {};
};



TEST(KeyValueElementTests, isKeyEqualTest) {
  Supla::KeyValueElement kve("secret");
  EXPECT_TRUE(kve.isKeyEqual("secret"));
  EXPECT_FALSE(kve.isKeyEqual("Secret"));
  EXPECT_FALSE(kve.isKeyEqual("secret "));
  EXPECT_FALSE(kve.isKeyEqual("secr"));
  EXPECT_FALSE(kve.isKeyEqual("test"));
}

TEST(KeyValueElementTests, elementSequenceTest) {
  Supla::KeyValueElement kve1("1");
  EXPECT_EQ(kve1.getNext(), nullptr);

  Supla::KeyValueElement kve2("2");
  EXPECT_EQ(kve1.getNext(), nullptr);
  EXPECT_EQ(kve2.getNext(), nullptr);

  kve1.add(&kve2);
  EXPECT_EQ(kve1.getNext(), &kve2);
  EXPECT_EQ(kve2.getNext(), nullptr);

  Supla::KeyValueElement kve3("3");
  EXPECT_EQ(kve1.getNext(), &kve2);
  EXPECT_EQ(kve2.getNext(), nullptr);
  EXPECT_EQ(kve3.getNext(), nullptr);

  kve1.add(&kve3);
  EXPECT_EQ(kve1.getNext(), &kve2);
  EXPECT_EQ(kve2.getNext(), &kve3);
  EXPECT_EQ(kve3.getNext(), nullptr);

  Supla::KeyValueElement kve4("4");
  EXPECT_EQ(kve1.getNext(), &kve2);
  EXPECT_EQ(kve2.getNext(), &kve3);
  EXPECT_EQ(kve3.getNext(), nullptr);
  EXPECT_EQ(kve4.getNext(), nullptr);

  kve3.add(&kve4);
  EXPECT_EQ(kve1.getNext(), &kve2);
  EXPECT_EQ(kve2.getNext(), &kve3);
  EXPECT_EQ(kve3.getNext(), &kve4);
  EXPECT_EQ(kve4.getNext(), nullptr);

}

TEST(KeyValueElementTests, gettersOnEmptyTest) {
  Supla::KeyValueElement kve1("1");
  Supla::KeyValueElement kve2("2");
  Supla::KeyValueElement kve3("3");
  Supla::KeyValueElement kve4("4");

  uint8_t resultU8 = 1;
  int8_t result8 = 2;
  uint32_t resultU32 = 3;
  int32_t result32 = 4;

  EXPECT_FALSE(kve1.getUInt8(&resultU8));
  EXPECT_EQ(resultU8, 1);

  EXPECT_FALSE(kve1.getInt8(&result8));
  EXPECT_EQ(result8, 2);

  EXPECT_FALSE(kve1.getUInt32(&resultU32));
  EXPECT_EQ(resultU32, 3);

  EXPECT_FALSE(kve1.getInt32(&result32));
  EXPECT_EQ(result32, 4);

  char temp[10] = {};

  EXPECT_FALSE(kve2.getBlob(temp, 10));
  EXPECT_FALSE(kve3.getString(temp, 10));
}

TEST(KeyValueElementTests, intTest) {
  Supla::KeyValueElement kve1("1");
  Supla::KeyValueElement kve2("2");
  Supla::KeyValueElement kve3("3");
  Supla::KeyValueElement kve4("4");

  uint8_t resultU8 = 1;
  int8_t result8 = 2;
  uint32_t resultU32 = 3;
  int32_t result32 = 4;

  // UINT 8
  EXPECT_TRUE(kve1.setUInt8(250));
  EXPECT_TRUE(kve1.getUInt8(&resultU8));
  EXPECT_EQ(resultU8, 250);

  EXPECT_FALSE(kve1.getInt8(&result8));
  EXPECT_EQ(result8, 2);

  EXPECT_FALSE(kve1.getUInt32(&resultU32));
  EXPECT_EQ(resultU32, 3);

  EXPECT_FALSE(kve1.getInt32(&result32));
  EXPECT_EQ(result32, 4);

  char temp[10] = {};

  EXPECT_FALSE(kve1.getBlob(temp, 10));
  EXPECT_FALSE(kve1.getString(temp, 10));

  EXPECT_TRUE(kve1.setUInt8(25));
  EXPECT_TRUE(kve1.getUInt8(&resultU8));
  EXPECT_EQ(resultU8, 25);

  // INT 8
  EXPECT_TRUE(kve2.setInt8(70));
  EXPECT_FALSE(kve2.getUInt8(&resultU8));
  EXPECT_EQ(resultU8, 25);

  EXPECT_TRUE(kve2.getInt8(&result8));
  EXPECT_EQ(result8, 70);

  EXPECT_FALSE(kve2.getUInt32(&resultU32));
  EXPECT_EQ(resultU32, 3);

  EXPECT_FALSE(kve2.getInt32(&result32));
  EXPECT_EQ(result32, 4);

  EXPECT_FALSE(kve2.getBlob(temp, 10));
  EXPECT_FALSE(kve2.getString(temp, 10));

  EXPECT_FALSE(kve2.setUInt8(26));
  EXPECT_FALSE(kve2.getUInt8(&resultU8));
  EXPECT_EQ(resultU8, 25);

  EXPECT_TRUE(kve2.setInt8(71));
  EXPECT_TRUE(kve2.getInt8(&result8));
  EXPECT_EQ(result8, 71);

  // UINT 32
  EXPECT_TRUE(kve3.setUInt32(12345));
  EXPECT_TRUE(kve3.getUInt32(&resultU32));
  EXPECT_EQ(resultU32, 12345);

  EXPECT_FALSE(kve3.getUInt8(&resultU8));
  EXPECT_FALSE(kve3.getInt8(&result8));
  EXPECT_FALSE(kve3.getInt32(&result32));

  EXPECT_FALSE(kve3.getBlob(temp, 10));
  EXPECT_FALSE(kve3.getString(temp, 10));

  EXPECT_FALSE(kve3.setUInt8(26));
  EXPECT_FALSE(kve3.getUInt8(&resultU8));

  EXPECT_FALSE(kve3.setInt8(71));
  EXPECT_FALSE(kve3.setBlob(temp, 10));
  EXPECT_FALSE(kve3.setString(temp));
  EXPECT_FALSE(kve3.getInt8(&result8));

  EXPECT_TRUE(kve3.setUInt32(50));
  EXPECT_TRUE(kve3.getUInt32(&resultU32));
  EXPECT_EQ(resultU32, 50);

  // INT 32
  EXPECT_TRUE(kve4.setInt32(-12345));
  EXPECT_TRUE(kve4.getInt32(&result32));
  EXPECT_EQ(result32, -12345);

  EXPECT_FALSE(kve4.getUInt8(&resultU8));
  EXPECT_FALSE(kve4.getInt8(&result8));
  EXPECT_FALSE(kve4.getUInt32(&resultU32));

  EXPECT_FALSE(kve4.getBlob(temp, 10));
  EXPECT_FALSE(kve4.getString(temp, 10));

  EXPECT_FALSE(kve4.setUInt8(26));
  EXPECT_FALSE(kve4.getUInt8(&resultU8));

  EXPECT_FALSE(kve4.setInt8(71));
  EXPECT_FALSE(kve4.getInt8(&result8));

  EXPECT_TRUE(kve4.setInt32(50));
  EXPECT_TRUE(kve4.getInt32(&result32));
  EXPECT_EQ(result32, 50);


}

TEST(KeyValueElementTests, stringTest) {
  Supla::KeyValueElement kve1("1");

  uint8_t resultU8 = 1;
  int8_t result8 = 2;
  uint32_t resultU32 = 3;
  int32_t result32 = 4;

  // UINT 8
  EXPECT_TRUE(kve1.setString("testing"));

  char temp[10] = {};

  EXPECT_FALSE(kve1.getBlob(temp, 10));
  EXPECT_TRUE(kve1.getString(temp, 10));
  EXPECT_EQ(strncmp(temp, "testing", 10), 0);

  EXPECT_FALSE(kve1.getUInt8(&resultU8));
  EXPECT_FALSE(kve1.getInt8(&result8));
  EXPECT_FALSE(kve1.getUInt32(&resultU32));
  EXPECT_FALSE(kve1.getInt32(&result32));
  EXPECT_FALSE(kve1.getBlob(temp, 10));

  EXPECT_FALSE(kve1.setUInt8(25));

  EXPECT_TRUE(kve1.setString("This is much longer string used for testing"));
  EXPECT_FALSE(kve1.getString(temp, 10));

  char temp2[100] = {};
  EXPECT_TRUE(kve1.getString(temp2, 100));
  EXPECT_EQ(strncmp(temp2, "This is much longer string used for testing", 100), 0);
}

TEST(KeyValueElementTests, blobTest) {
  Supla::KeyValueElement kve1("testing");

  uint8_t resultU8 = 1;
  int8_t result8 = 2;
  uint32_t resultU32 = 3;
  int32_t result32 = 4;

  // UINT 8
  EXPECT_TRUE(kve1.setBlob("testing", 4));

  char temp[10] = {};

  EXPECT_FALSE(kve1.getBlob(temp, 10));
  EXPECT_FALSE(kve1.getBlob(temp, 3));
  EXPECT_FALSE(kve1.getBlob(temp, 5));
  EXPECT_TRUE(kve1.getBlob(temp, 4));
  EXPECT_EQ(strncmp(temp, "test", 4), 0);

  EXPECT_FALSE(kve1.getUInt8(&resultU8));
  EXPECT_FALSE(kve1.getInt8(&result8));
  EXPECT_FALSE(kve1.getUInt32(&resultU32));
  EXPECT_FALSE(kve1.getInt32(&result32));
  EXPECT_FALSE(kve1.getString(temp, 10));

  EXPECT_FALSE(kve1.setUInt8(25));

  EXPECT_TRUE(kve1.setBlob("1234567890", 10));
  EXPECT_FALSE(kve1.getBlob(temp, 9));
  EXPECT_TRUE(kve1.getBlob(temp, 10));
  EXPECT_EQ(strncmp(temp, "1234567890", 10), 0);

  uint8_t expectedData[] = {'t', 'e', 's', 't', 'i', 'n', 'g', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', // key
    5, // dataType
    10, 0, // size -- this part is endian dependent - test will fail if it is
           // run on machine with different endian.
   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0' // data
  };

  uint8_t serializedData[100] = {};
  EXPECT_TRUE(kve1.serialize(serializedData, sizeof(serializedData)));
  EXPECT_EQ(memcmp(expectedData, serializedData, sizeof(expectedData)), 0);
  EXPECT_FALSE(kve1.serialize(serializedData, 10));
}

TEST(KeyValueTests, integrationTest) {
  KeyValueTest kvStorage;

  uint8_t resultU8 = 1;
  int8_t result8 = 2;
  uint32_t resultU32 = 3;
  int32_t result32 = 4;
  char temp[50];

  // checks on empty storage
  EXPECT_FALSE(kvStorage.getInt8("test", &result8));
  EXPECT_FALSE(kvStorage.getUInt8("", &resultU8));
  EXPECT_FALSE(kvStorage.getInt32("test", &result32));
  EXPECT_FALSE(kvStorage.getUInt32("test", &resultU32));
  EXPECT_FALSE(kvStorage.getString("test", temp, 20));
  EXPECT_FALSE(kvStorage.getBlob("test", temp, 25));

  EXPECT_TRUE(kvStorage.setString("wifissid", "MY wiFi SSID"));
  EXPECT_FALSE(kvStorage.getString("wifissid2", temp, 50));
  EXPECT_TRUE(kvStorage.getString("wifissid", temp, 50));
  EXPECT_STREQ(temp, "MY wiFi SSID");

  EXPECT_FALSE(kvStorage.setUInt8("wifissid", 32));
  EXPECT_TRUE(kvStorage.getString("wifissid", temp, 50));
  EXPECT_STREQ(temp, "MY wiFi SSID");

  EXPECT_TRUE(kvStorage.setString("passwd", "secret"));
  EXPECT_FALSE(kvStorage.getString("passwd", temp, 5));
  EXPECT_STREQ(temp, "MY wiFi SSID");
  EXPECT_TRUE(kvStorage.getString("passwd", temp, 50));
  EXPECT_STREQ(temp, "secret");

  EXPECT_TRUE(kvStorage.setUInt32("suplaport", 2019));
  EXPECT_TRUE(kvStorage.getUInt32("suplaport", &resultU32));
  EXPECT_EQ(resultU32, 2019);

  EXPECT_TRUE(kvStorage.getString("wifissid", temp, 50));
  EXPECT_STREQ(temp, "MY wiFi SSID");


  uint8_t buffer[1024] = {};
  size_t dataWritten = kvStorage.serializeToMemory(buffer, 1024);
  EXPECT_EQ(dataWritten,
      15 + 1 + 2 + 13 +  // wifissid
      15 + 1 + 2 + 7 +   // passwdf
      15 + 1 + 2 + 4     // suplaport
      );


  KeyValueTest kvStorageRestored;
  EXPECT_TRUE(kvStorageRestored.initFromMemory(buffer, dataWritten));

  EXPECT_TRUE(kvStorageRestored.getString("wifissid", temp, 50));
  EXPECT_STREQ(temp, "MY wiFi SSID");
  EXPECT_TRUE(kvStorageRestored.getString("passwd", temp, 50));
  EXPECT_STREQ(temp, "secret");
  EXPECT_TRUE(kvStorageRestored.getUInt32("suplaport", &resultU32));
  EXPECT_EQ(resultU32, 2019);

  uint8_t secondBuffer[1024] = {};
  size_t secondDataWritten = kvStorageRestored.serializeToMemory(
      secondBuffer, 1024);

  EXPECT_EQ(secondDataWritten, dataWritten);
  // make sure that serialized data is the same after serialization ->
  // deserialization -> serialization
  EXPECT_EQ(memcmp(buffer, secondBuffer, 1024), 0);
}

TEST(KeyValueTests, variousKVChecks) {
  KeyValueTest kvStorage;

  int8_t result8 = {};
  uint8_t resultU8 = {};
  uint32_t resultU32 = {};
  int32_t result32 = {};

  EXPECT_TRUE(kvStorage.setInt8("this is too long key", 13));
  EXPECT_TRUE(kvStorage.getInt8("this is too long key", &result8));
  EXPECT_EQ(result8, 13);
  // different 15th char
  EXPECT_FALSE(kvStorage.getInt8("this is too low", &result8));

  EXPECT_TRUE(kvStorage.setEmail("this_is_mail@user.com"));
  char buf[2000] = {};
  EXPECT_TRUE(kvStorage.getEmail(buf));
  EXPECT_STREQ(buf, "this_is_mail@user.com");

  //generateGuidAndAuthkey();
  EXPECT_TRUE(kvStorage.setDeviceName("device name"));
  EXPECT_TRUE(kvStorage.setDeviceMode(Supla::DEVICE_MODE_NORMAL));
  EXPECT_TRUE(kvStorage.setGUID("1234567890"));

  EXPECT_TRUE(kvStorage.setSwUpdateServer("update.server"));
  EXPECT_TRUE(kvStorage.setSwUpdateBeta(true));
  EXPECT_TRUE(kvStorage.isSwUpdateBeta());

  EXPECT_TRUE(kvStorage.setSuplaCommProtocolEnabled(false));
  EXPECT_FALSE(kvStorage.isSuplaCommProtocolEnabled());

  EXPECT_TRUE(kvStorage.setSuplaServer("supla.server"));
  EXPECT_TRUE(kvStorage.setSuplaServerPort(1234));
  EXPECT_TRUE(kvStorage.setAuthKey("0987654321"));

  EXPECT_TRUE(kvStorage.getSuplaServer(buf));
  EXPECT_STREQ(buf, "supla.server");
  EXPECT_EQ(kvStorage.getSuplaServerPort(), 1234);
  EXPECT_TRUE(kvStorage.getAuthKey(buf));
  EXPECT_STREQ(buf, "0987654321");
  EXPECT_TRUE(kvStorage.getDeviceName(buf));
  EXPECT_STREQ(buf, "device name");
  EXPECT_EQ(kvStorage.getDeviceMode(), Supla::DEVICE_MODE_NORMAL);
  EXPECT_TRUE(kvStorage.getGUID(buf));
  EXPECT_STREQ(buf, "1234567890");
  EXPECT_TRUE(kvStorage.getSwUpdateServer(buf));
  EXPECT_STREQ(buf, "update.server");


  EXPECT_TRUE(kvStorage.setMqttCommProtocolEnabled(true));
  EXPECT_TRUE(kvStorage.setMqttServer("mqtt.server"));
  EXPECT_TRUE(kvStorage.setMqttServerPort(42));
  EXPECT_TRUE(kvStorage.setMqttUser("mqtt user"));
  EXPECT_TRUE(kvStorage.setMqttPassword("mqtt pass"));
  EXPECT_TRUE(kvStorage.setMqttQos(2));
  EXPECT_TRUE(kvStorage.setMqttTlsEnabled(true));
  EXPECT_TRUE(kvStorage.setMqttAuthEnabled(false));
  EXPECT_TRUE(kvStorage.setMqttRetainEnabled(true));
  EXPECT_TRUE(kvStorage.setMqttPrefix("supla test"));

  EXPECT_TRUE(kvStorage.isMqttCommProtocolEnabled());
  EXPECT_TRUE(kvStorage.isMqttTlsEnabled());
  EXPECT_FALSE(kvStorage.isMqttAuthEnabled());
  EXPECT_TRUE(kvStorage.isMqttRetainEnabled());

  EXPECT_TRUE(kvStorage.getMqttServer(buf));
  EXPECT_STREQ(buf, "mqtt.server");
  EXPECT_EQ(kvStorage.getMqttServerPort(), 42);
  EXPECT_TRUE(kvStorage.getMqttUser(buf));
  EXPECT_STREQ(buf, "mqtt user");
  EXPECT_TRUE(kvStorage.getMqttPassword(buf));
  EXPECT_STREQ(buf, "mqtt pass");
  EXPECT_EQ(kvStorage.getMqttQos(), 2);
  EXPECT_TRUE(kvStorage.getMqttPrefix(buf));
  EXPECT_STREQ(buf, "supla test");

  EXPECT_TRUE(kvStorage.setWiFiSSID("ssid"));
  EXPECT_TRUE(kvStorage.setWiFiPassword("pass"));
  EXPECT_TRUE(kvStorage.setAltWiFiSSID("altssid"));
  EXPECT_TRUE(kvStorage.setAltWiFiPassword("altpass"));

  EXPECT_TRUE(kvStorage.getWiFiSSID(buf));
  EXPECT_STREQ(buf, "ssid");
  EXPECT_TRUE(kvStorage.getWiFiPassword(buf));
  EXPECT_STREQ(buf, "pass");
  EXPECT_TRUE(kvStorage.getAltWiFiSSID(buf));
  EXPECT_STREQ(buf, "altssid");
  EXPECT_TRUE(kvStorage.getAltWiFiPassword(buf));
  EXPECT_STREQ(buf, "altpass");

}

