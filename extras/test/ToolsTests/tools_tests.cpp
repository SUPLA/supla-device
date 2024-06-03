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
#include <supla/tools.h>

TEST(ToolsTests, isArrayEmptyTests) {
  char buf[10] = {};
  char buf2[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  char buf3[10] = {40, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  char buf4[1000] = {2};
  char buf5[1000] = {};

  EXPECT_TRUE(isArrayEmpty(buf, 10));
  buf[4] = 3;
  EXPECT_FALSE(isArrayEmpty(buf, 10));

  EXPECT_FALSE(isArrayEmpty(buf2, 10));
  EXPECT_TRUE(isArrayEmpty(buf2, 9));

  EXPECT_FALSE(isArrayEmpty(buf3, 10));
  EXPECT_FALSE(isArrayEmpty(buf3, 9));

  EXPECT_FALSE(isArrayEmpty(buf4, 1000));
  EXPECT_TRUE(isArrayEmpty(buf5, 1000));
}

TEST(ToolsTests, adjustRangeTests) {
  EXPECT_EQ(adjustRange(10, 0, 10, 50, 100), 100);
  EXPECT_EQ(adjustRange(5, 0, 10, 50, 100), 75);
  EXPECT_EQ(adjustRange(0, 0, 10, 50, 100), 50);

  EXPECT_EQ(adjustRange(0, 0, 10, -50, -100), -50);
  EXPECT_EQ(adjustRange(0, 0, 10, -100, -50), -100);

  EXPECT_EQ(adjustRange(-5, 0, 10, 50, 100), 25);
}

TEST(ToolsTests, generateHexStringTests) {
  char buf[200];
  EXPECT_EQ(generateHexString("SUPLA", buf, 5), 10);
  EXPECT_STREQ(buf, "5355504C41");
  EXPECT_EQ(generateHexString("SUPLA", buf, 5, ':'), 14);
  EXPECT_STREQ(buf, "53:55:50:4C:41");
  EXPECT_EQ(generateHexString("", buf, 0, ':'), 0);
  EXPECT_STREQ(buf, "");
  EXPECT_EQ(generateHexString("", buf, 1, ':'), 2);
  EXPECT_STREQ(buf, "00");
}

TEST(ToolsTests, hexStringToIntTests) {
  EXPECT_EQ(hexStringToInt("10", 2), 16);
  EXPECT_EQ(hexStringToInt("A0", 2), 160);
  EXPECT_EQ(hexStringToInt("a5", 2), 165);
  EXPECT_EQ(hexStringToInt("5", 1), 5);
  EXPECT_EQ(hexStringToInt("05", 2), 5);
  EXPECT_EQ(hexStringToInt("0a", 2), 10);
  EXPECT_EQ(hexStringToInt("0A", 2), 10);
  EXPECT_EQ(hexStringToInt("FF", 2), 255);
  EXPECT_EQ(hexStringToInt("fF", 2), 255);
  EXPECT_EQ(hexStringToInt("ff", 2), 255);
  EXPECT_EQ(hexStringToInt("0ff", 3), 255);
  EXPECT_EQ(hexStringToInt("1ff", 3), 511);
  EXPECT_EQ(hexStringToInt("45FAc21", 7), 73378849);

  // uint32_t max
  EXPECT_EQ(hexStringToInt("FFFFFFFF", 8), 4294967295);
  // out of uint32_t limit call
  hexStringToInt("FFFFFFFF2", 9);
}

TEST(ToolsTest, urlDecodeInplaceTests) {
  {
    char buf[] = "%61la ma+supla%1";
    urlDecodeInplace(buf, sizeof(buf) - 1);
    EXPECT_STREQ(buf, "ala ma supla");
  }

  {
    char buf[] = "ala+ma+supla";
    urlDecodeInplace(buf, sizeof(buf) - 1);
    EXPECT_STREQ(buf, "ala ma supla");
  }

  {
    char buf[] = "ala+ma%20supla";
    urlDecodeInplace(buf, sizeof(buf) - 1);
    EXPECT_STREQ(buf, "ala ma supla");
  }

  {
    char buf[] = "ala+ma+supla";
    urlDecodeInplace(buf, sizeof(buf) - 1);
    EXPECT_STREQ(buf, "ala ma supla");
  }

  {
    char buf[] = "ala+ma+supla";
    urlDecodeInplace(buf, sizeof(buf) - 1);
    EXPECT_STREQ(buf, "ala ma supla");
  }

}

TEST(ToolsTest, urlEncodeTests) {
  {
    char input[] = "ala ma supla";
    char output[1024] = {};
    EXPECT_EQ(urlEncode(input, output, 1024), 16);
    EXPECT_STREQ(output, "ala%20ma%20supla");
  }

  {
    // unreserved chars only
    char input[] = "azAZ01234567890~.-_";
    char output[1024] = {};
    EXPECT_EQ(urlEncode(input, output, 1024), 19);
    EXPECT_STREQ(output, input);
  }

  {
    char input[] = "~@#$   +~-';][/., ala MA supla";
    char output[1024] = {};
    EXPECT_EQ(urlEncode(input, output, 1024), 62);
    EXPECT_STREQ(output, "~%40%23%24%20%20%20%2B~-%27%3B%5D%5B%2F.%2C%20ala%20MA%20supla");
  }

  {
    char input[] = "AlA  ma supla";
    char output[10] = {};
    EXPECT_EQ(urlEncode(input, output, 10), 9);
    EXPECT_STREQ(output, "AlA%20%20");
  }

  {
    char input[] = "ala 1 ma supla";
    char output[10] = {};
    EXPECT_EQ(urlEncode(input, output, 10), 7);
    EXPECT_STREQ(output, "ala%201");
  }

  {
    char input[] = "ala  ma supla";
    char output[11] = {};
    EXPECT_EQ(urlEncode(input, output, 11), 10);
    EXPECT_STREQ(output, "ala%20%20m");
  }

  {
    char input[] = "ala ma suplę";
    char output[1024] = {};
    EXPECT_EQ(urlEncode(input, output, 1024), 21);
    EXPECT_STREQ(output, "ala%20ma%20supl%C4%99");
  }

  {
    char input[] = "";
    char output[1024] = {};
    EXPECT_EQ(urlEncode(input, output, 1024), 0);
    EXPECT_STREQ(output, "");
  }

}

TEST(ToolsTest, strncmpInsensitiveTests) {
  {
    char s1[] = "ala ma supla";
    char s2[] = "test";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), -1);
  }

  {
    char s1[] = "testujemy";
    char s2[] = "test";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), 0);
  }

  {
    char *s1 = nullptr;
    char s2[] = "test";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), -1);
  }

  {
    char *s1 = nullptr;
    char *s2 = nullptr;
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), 0);
  }

  {
    char s1[] = "test";
    char *s2 = nullptr;
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), 1);
  }

  {
    char s1[] = "supla testujemy";
    char s2[] = "test";
    EXPECT_EQ(strncmpInsensitive(s1 + 6, s2, 4), 0);
  }

  {
    char s1[] = "supla testujemy";
    char s2[] = "test";
    EXPECT_EQ(strncmpInsensitive(s1 + 6, s2, 5), 1);
  }

  {
    char s1[] = "Test";
    char s2[] = "test";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), 0);
  }

  {
    char s1[] = "Test";
    char s2[] = "TEST";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), 0);
  }

  {
    char s1[] = "Test";
    char s2[] = "teST";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), 0);
  }

  {
    char s1[] = "test";
    char s2[] = "tESt";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 4), 0);
  }

  {
    char s1[] = "123test";
    char s2[] = "123tESt";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 7), 0);
  }

  {
    char s1[] = "123test";
    char s2[] = "123tESt";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 8), 0);
  }

  {
    char s1[] = "123te";
    char s2[] = "123tESt";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 8), -1);
  }

  {
    char s1[] = "123te";
    char s2[] = "123tESt";
    EXPECT_EQ(strncmpInsensitive(s1, s2, 5), 0);
  }

  {
    char s1[] = "this.is.a.test.WiTh.Dots.org";
    char s2[] = ".with.dots.org";
    EXPECT_EQ(strncmpInsensitive(s1 + 14, s2, 6), 0);
  }

  {
    char s1[] = "this.is.a.test.WiTh.Dots.org";
    char s2[] = ".with.dots.org";
    int size1 = strlen(s1);
    int size2 = strlen(s2);

    EXPECT_EQ(strncmpInsensitive(s1 + size1 - size2, s2, size2), 0);
  }
}

TEST(ToolsTests, floatStringToIntTests) {
  EXPECT_EQ(floatStringToInt("10", 2), 1000);
  EXPECT_EQ(floatStringToInt("3,1415", 3), 3141);
  EXPECT_EQ(floatStringToInt("3,1415", 4), 31415);
  EXPECT_EQ(floatStringToInt("-3,1415", 4), -31415);
  EXPECT_EQ(floatStringToInt("-3,1415", 3), -3141);
  EXPECT_EQ(floatStringToInt("-100", 2), -10000);
  EXPECT_EQ(floatStringToInt("-1", 1), -10);
  EXPECT_EQ(floatStringToInt("-1", 0), -1);
  EXPECT_EQ(floatStringToInt("0", 0), 0);
  EXPECT_EQ(floatStringToInt("0", 3), 0);
  EXPECT_EQ(floatStringToInt("1", 3), 1000);
}

TEST(ToolsTests, stringToIntTests) {
  EXPECT_EQ(stringToInt("10", 2), 10);
  EXPECT_EQ(stringToInt("A0", 2), 0);
  EXPECT_EQ(stringToInt("a5", 2), 0);
  EXPECT_EQ(stringToInt("-5", 1), 0);
  EXPECT_EQ(stringToInt("05"), 5);
  EXPECT_EQ(stringToInt("0-2"), 0);
  EXPECT_EQ(stringToInt("-1"), -1);
  EXPECT_EQ(stringToInt(""), 0);
  EXPECT_EQ(stringToInt("-1234.2"), 0);
  EXPECT_EQ(stringToInt("-1234"), -1234);
  EXPECT_EQ(stringToInt("1234"), 1234);
  EXPECT_EQ(stringToInt("-1-"), 0);
}

TEST(ToolsTest, stringToColorTests) {
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;

  EXPECT_FALSE(stringToColor("this is not valid", &red, &green, &blue));
  EXPECT_EQ(red, 0);
  EXPECT_EQ(green, 0);
  EXPECT_EQ(blue, 0);
  EXPECT_FALSE(stringToColor("200", &red, &green, &blue));
  EXPECT_EQ(red, 0);
  EXPECT_EQ(green, 0);
  EXPECT_EQ(blue, 0);
  EXPECT_FALSE(stringToColor("10,20", &red, &green, &blue));
  EXPECT_EQ(red, 0);
  EXPECT_EQ(green, 0);
  EXPECT_EQ(blue, 0);
  EXPECT_FALSE(stringToColor("10,20 30", &red, &green, &blue));
  EXPECT_EQ(red, 0);
  EXPECT_EQ(green, 0);
  EXPECT_EQ(blue, 0);

  EXPECT_TRUE(stringToColor("10,20,30", &red, &green, &blue));
  EXPECT_EQ(red, 10);
  EXPECT_EQ(green, 20);
  EXPECT_EQ(blue, 30);

  EXPECT_FALSE(stringToColor("10,20,-30", &red, &green, &blue));
  EXPECT_EQ(red, 10);
  EXPECT_EQ(green, 20);
  EXPECT_EQ(blue, 30);

  EXPECT_FALSE(stringToColor("10,20,300", &red, &green, &blue));
  EXPECT_EQ(red, 10);
  EXPECT_EQ(green, 20);
  EXPECT_EQ(blue, 30);

  EXPECT_FALSE(stringToColor("1000,20,30", &red, &green, &blue));
  EXPECT_EQ(red, 10);
  EXPECT_EQ(green, 20);
  EXPECT_EQ(blue, 30);

  EXPECT_TRUE(stringToColor("0,0,0", &red, &green, &blue));
  EXPECT_EQ(red, 0);
  EXPECT_EQ(green, 0);
  EXPECT_EQ(blue, 0);

  EXPECT_TRUE(stringToColor("255,255,255", &red, &green, &blue));
  EXPECT_EQ(red, 255);
  EXPECT_EQ(green, 255);
  EXPECT_EQ(blue, 255);
}

TEST(ToolsTest, rssiToSignalStrengthTests) {
  EXPECT_EQ(Supla::rssiToSignalStrength(0), 100);
  EXPECT_EQ(Supla::rssiToSignalStrength(10), 100);
  EXPECT_EQ(Supla::rssiToSignalStrength(-100), 0);
  EXPECT_EQ(Supla::rssiToSignalStrength(-100000), 0);
  EXPECT_EQ(Supla::rssiToSignalStrength(-50), 100);
  EXPECT_EQ(Supla::rssiToSignalStrength(-49), 100);
  EXPECT_EQ(Supla::rssiToSignalStrength(-51), 98);
  EXPECT_EQ(Supla::rssiToSignalStrength(-75), 50);
  EXPECT_EQ(Supla::rssiToSignalStrength(-95), 10);
  EXPECT_EQ(Supla::rssiToSignalStrength(-99), 2);
}
