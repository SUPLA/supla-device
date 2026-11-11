// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/parser/simple.h>

#include <cmath>
#include <limits>
#include <string>
#include <variant>

namespace {

class TestSource : public Supla::Source::Source {
 public:
  std::string getContent() override { return content; }
  std::string content;
};

class InspectableSimple : public Supla::Parser::Simple {
 public:
  explicit InspectableSimple(Supla::Source::Source *source) : Simple(source) {}
  static constexpr size_t limit = MAX_FIELDS;
  size_t fieldCount() const { return values.size(); }
  size_t fieldCapacity() const { return values.capacity(); }
  size_t keyCount() const { return keys.size(); }
};

class SimpleParserTests : public testing::Test {
 protected:
  TestSource source;
  InspectableSimple parser{&source};
};

}  // namespace

TEST_F(SimpleParserTests, ParsesExistingNumericBooleanAndTextTypes) {
  source.content = "42\n-12\n2.75\ntrue\nfalse\nhello\n 12suffix\n0x10\n";
  for (int i = 0; i < 8; i++) {
    parser.addKey(std::to_string(i), i);
  }
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_TRUE(parser.isBasedOnIndex());
  EXPECT_EQ(std::get<int>(parser.getStateValue("0")), 42);
  EXPECT_EQ(std::get<int>(parser.getStateValue("1")), -12);
  EXPECT_DOUBLE_EQ(parser.getValue("2"), 2.75);
  EXPECT_EQ(std::get<int>(parser.getStateValue("2")), 2);
  EXPECT_TRUE(std::get<bool>(parser.getStateValue("3")));
  EXPECT_FALSE(std::get<bool>(parser.getStateValue("4")));
  EXPECT_EQ(std::get<std::string>(parser.getStateValue("5")), "hello");
  EXPECT_EQ(std::get<int>(parser.getStateValue("6")), 12);
  EXPECT_DOUBLE_EQ(parser.getValue("7"), 16);
  EXPECT_TRUE(parser.isValid());
}

TEST_F(SimpleParserTests, PreservesEmptyFieldsAndTrailingSeparatorSemantics) {
  source.content = "\n11\n\n22\n\n";
  for (int i = 0; i < 5; i++) {
    parser.addKey(std::to_string(i), i);
  }
  ASSERT_TRUE(parser.refreshSource());
  ASSERT_EQ(parser.fieldCount(), 5u);
  EXPECT_EQ(std::get<std::string>(parser.getStateValue("0")), "");
  EXPECT_DOUBLE_EQ(parser.getValue("1"), 11);
  EXPECT_EQ(std::get<std::string>(parser.getStateValue("2")), "");
  EXPECT_DOUBLE_EQ(parser.getValue("3"), 22);
  EXPECT_EQ(std::get<std::string>(parser.getStateValue("4")), "");

  source.content = "11\n22\n";
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_EQ(parser.fieldCount(), 2u);
  source.content = "11\n22";
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_EQ(parser.fieldCount(), 2u);
}

TEST_F(SimpleParserTests, AcceptsExactlyTheFieldLimit) {
  source.content = std::string(InspectableSimple::limit - 1, '\n') + "9";
  parser.addKey("last", InspectableSimple::limit - 1);
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_EQ(parser.fieldCount(), InspectableSimple::limit);
  EXPECT_LE(parser.fieldCapacity(), InspectableSimple::limit);
  EXPECT_DOUBLE_EQ(parser.getValue("last"), 9);
  source.content.push_back('\n');
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_EQ(parser.fieldCount(), InspectableSimple::limit);
}

TEST_F(SimpleParserTests, RejectsOneExtraFieldBeforeAllocatingValues) {
  source.content = std::string(InspectableSimple::limit, '\n') + "9";
  EXPECT_FALSE(parser.refreshSource());
  EXPECT_FALSE(parser.isValid());
  EXPECT_EQ(parser.fieldCount(), 0u);
  EXPECT_EQ(parser.fieldCapacity(), 0u);
}

TEST_F(SimpleParserTests, RejectsOneMiBOfSeparatorsBeforeAllocatingValues) {
  source.content.assign(1024 * 1024, '\n');
  EXPECT_FALSE(parser.refreshSource());
  EXPECT_FALSE(parser.isValid());
  EXPECT_EQ(parser.fieldCount(), 0u);
  EXPECT_EQ(parser.fieldCapacity(), 0u);
}

TEST_F(SimpleParserTests, RejectsMissingKeysAndInvalidIndicesWithoutInsertion) {
  source.content = "42";
  parser.addKey("negative", -1);
  parser.addKey("pastEnd", 1);
  parser.addKey("huge", std::numeric_limits<int>::max());
  for (const char *key : {"missing", "negative", "pastEnd", "huge"}) {
    ASSERT_TRUE(parser.refreshSource());
    EXPECT_DOUBLE_EQ(parser.getValue(key), 0);
    EXPECT_FALSE(parser.isValid());
    ASSERT_TRUE(parser.refreshSource());
    EXPECT_EQ(std::get<int>(parser.getStateValue(key)), 0);
    EXPECT_FALSE(parser.isValid());
    EXPECT_EQ(parser.keyCount(), 3u);
  }
}

TEST_F(SimpleParserTests, RejectionClearsOldAndPartialValuesAndRecovers) {
  parser.addKey("first", 0);
  parser.addKey("second", 1);
  source.content = "41\n42";
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_DOUBLE_EQ(parser.getValue("first"), 41);
  source.content = "99\n" + std::string(InspectableSimple::limit, '\n');
  ASSERT_FALSE(parser.refreshSource());
  EXPECT_EQ(parser.fieldCount(), 0u);
  EXPECT_DOUBLE_EQ(parser.getValue("first"), 0);
  EXPECT_EQ(std::get<int>(parser.getStateValue("second")), 0);
  EXPECT_FALSE(parser.isValid());
  source.content = "51\n52";
  ASSERT_TRUE(parser.refreshSource());
  EXPECT_TRUE(parser.isValid());
  EXPECT_DOUBLE_EQ(parser.getValue("first"), 51);
  EXPECT_DOUBLE_EQ(parser.getValue("second"), 52);
}

TEST_F(SimpleParserTests, EmptyAndMissingSourcesAreInvalid) {
  parser.addKey("first", 0);
  source.content = "42";
  ASSERT_TRUE(parser.refreshSource());
  source.content.clear();
  EXPECT_FALSE(parser.refreshSource());
  EXPECT_EQ(parser.fieldCount(), 0u);
  EXPECT_DOUBLE_EQ(parser.getValue("first"), 0);
  EXPECT_FALSE(parser.isValid());
  InspectableSimple missing(nullptr);
  missing.addKey("first", 0);
  EXPECT_FALSE(missing.refreshSource());
  EXPECT_EQ(std::get<int>(missing.getStateValue("first")), 0);
  EXPECT_FALSE(missing.isValid());
}

TEST_F(SimpleParserTests, IntBoundariesRemainRepresentable) {
  parser.addKey("value", 0);
  for (int value : {std::numeric_limits<int>::min(),
                   std::numeric_limits<int>::max()}) {
    source.content = std::to_string(value);
    ASSERT_TRUE(parser.refreshSource());
    EXPECT_EQ(std::get<int>(parser.getStateValue("value")), value);
    EXPECT_TRUE(parser.isValid());
  }
}

TEST_F(SimpleParserTests, RejectsUnsafeNumericStateConversions) {
  parser.addKey("value", 0);
  for (const char *value : {"2147483648", "-2147483649", "1e100",
                            "nan", "inf", "-inf"}) {
    SCOPED_TRACE(value);
    source.content = value;
    ASSERT_TRUE(parser.refreshSource());
    EXPECT_EQ(std::get<int>(parser.getStateValue("value")), 0);
    EXPECT_FALSE(parser.isValid());
    ASSERT_TRUE(parser.refreshSource());
    const double number = parser.getValue("value");
    EXPECT_TRUE(std::isfinite(number));
    if (source.content == "nan" || source.content == "inf" ||
        source.content == "-inf") {
      EXPECT_DOUBLE_EQ(number, 0);
      EXPECT_FALSE(parser.isValid());
    } else {
      EXPECT_TRUE(parser.isValid());
    }
  }
}

TEST_F(SimpleParserTests, OutOfDoubleRangeAndInvalidNumbersRemainText) {
  parser.addKey("value", 0);
  for (const char *value : {"1e999", "1e-9999", "", "words", "12e+"}) {
    source.content = std::string(value) + "\n";
    ASSERT_TRUE(parser.refreshSource());
    if (source.content == "12e+\n") {
      EXPECT_EQ(std::get<int>(parser.getStateValue("value")), 12);
    } else {
      EXPECT_EQ(std::get<std::string>(parser.getStateValue("value")), value);
    }
    EXPECT_TRUE(parser.isValid());
  }
}
