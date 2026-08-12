// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/network/html/custom_checkbox_parameter.h>
#include <supla/network/html/select_input_parameter.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>

#include <cstring>
#include <string>
#include <vector>

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

class SenderMock : public Supla::WebSender {
 public:
  MOCK_METHOD(void, send, (const char*, int), (override));
};

class CustomCheckboxParameterProbe
    : public Supla::Html::CustomCheckboxParameter {
 public:
  using CustomCheckboxParameter::CustomCheckboxParameter;

  const char* tagValue() const {
    return tag;
  }

  const char* labelValue() const {
    return label;
  }
};

class SelectInputParameterProbe : public Supla::Html::SelectInputParameter {
 public:
  using SelectInputParameter::SelectInputParameter;

  const char* tagValue() const {
    return tag;
  }

  const char* labelValue() const {
    return label;
  }
};

static std::string makeTag(size_t size, char value = 'x') {
  return std::string(size, value);
}

static std::string expectedTag(const std::string& input) {
  const size_t limit = SUPLA_CONFIG_MAX_KEY_SIZE - 1;
  return input.substr(0, input.size() >= SUPLA_CONFIG_MAX_KEY_SIZE
                              ? limit
                              : input.size());
}

TEST(CustomCheckboxParameterTests, NullAndEmptyValuesClearExistingBuffers) {
  CustomCheckboxParameterProbe param("old_tag", "old_label");

  param.setTag(nullptr);
  param.setLabel(nullptr);

  EXPECT_EQ(param.tagValue(), nullptr);
  EXPECT_EQ(param.labelValue(), nullptr);

  param.setTag("");
  param.setLabel("");

  EXPECT_EQ(param.tagValue(), nullptr);
  EXPECT_EQ(param.labelValue(), nullptr);
}

TEST(CustomCheckboxParameterTests, TagsAreTerminatedAndTruncatedAtConfigLimit) {
  CustomCheckboxParameterProbe param(nullptr, "label");

  const std::vector<size_t> sizes = {
      0,
      SUPLA_CONFIG_MAX_KEY_SIZE - 2,
      SUPLA_CONFIG_MAX_KEY_SIZE - 1,
      SUPLA_CONFIG_MAX_KEY_SIZE,
      SUPLA_CONFIG_MAX_KEY_SIZE + 3,
  };

  for (const size_t size : sizes) {
    const std::string input = makeTag(size);
    param.setTag(input.c_str());
    const std::string expected = expectedTag(input);

    if (expected.empty()) {
      EXPECT_EQ(param.tagValue(), nullptr);
      continue;
    }

    ASSERT_NE(param.tagValue(), nullptr);
    EXPECT_EQ(std::strlen(param.tagValue()), expected.size());
    EXPECT_EQ(std::string(param.tagValue()), expected);
    EXPECT_EQ(param.tagValue()[expected.size()], '\0');
  }
}

TEST(CustomCheckboxParameterTests, RendersAndStoresUsingTruncatedTag) {
  ConfigMock cfg;
  SenderMock sender;
  const std::string input = makeTag(SUPLA_CONFIG_MAX_KEY_SIZE + 3, 'c');
  const std::string tag = expectedTag(input);
  std::string html;

  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq(tag), _))
      .WillOnce([](const char*, uint8_t* value) {
        *value = 1;
        return true;
      });
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly([&html](const char* data, int size) {
        html.append(data, size < 0 ? std::strlen(data)
                                   : static_cast<size_t>(size));
      });

  CustomCheckboxParameterProbe param(input.c_str(), "label");
  param.send(&sender);

  EXPECT_THAT(html, ::testing::HasSubstr("name=\"" + tag + "\""));
  EXPECT_THAT(html, ::testing::HasSubstr("id=\"" + tag + "\""));
  EXPECT_THAT(html, ::testing::HasSubstr("for=\"" + tag + "\""));

  EXPECT_CALL(cfg, setUInt8(StrEq(tag), 1)).Times(1);
  EXPECT_TRUE(param.handleResponse(tag.c_str(), "on"));
}

TEST(SelectInputParameterTests, NullAndEmptyValuesClearExistingBuffers) {
  SelectInputParameterProbe param("old_tag", "old_label");

  param.setTag(nullptr);
  param.setLabel(nullptr);

  EXPECT_EQ(param.tagValue(), nullptr);
  EXPECT_EQ(param.labelValue(), nullptr);

  param.setTag("");
  param.setLabel("");

  EXPECT_EQ(param.tagValue(), nullptr);
  EXPECT_EQ(param.labelValue(), nullptr);
}

TEST(SelectInputParameterTests, TagsAreTerminatedAndTruncatedAtConfigLimit) {
  SelectInputParameterProbe param;

  const std::vector<size_t> sizes = {
      0,
      SUPLA_CONFIG_MAX_KEY_SIZE - 2,
      SUPLA_CONFIG_MAX_KEY_SIZE - 1,
      SUPLA_CONFIG_MAX_KEY_SIZE,
      SUPLA_CONFIG_MAX_KEY_SIZE + 3,
  };

  for (const size_t size : sizes) {
    const std::string input = makeTag(size, 's');
    param.setTag(input.c_str());
    const std::string expected = expectedTag(input);

    if (expected.empty()) {
      EXPECT_EQ(param.tagValue(), nullptr);
      continue;
    }

    ASSERT_NE(param.tagValue(), nullptr);
    EXPECT_EQ(std::strlen(param.tagValue()), expected.size());
    EXPECT_EQ(std::string(param.tagValue()), expected);
    EXPECT_EQ(param.tagValue()[expected.size()], '\0');
  }
}

TEST(SelectInputParameterTests, RendersAndStoresUsingTruncatedTag) {
  ConfigMock cfg;
  SenderMock sender;
  const std::string input = makeTag(SUPLA_CONFIG_MAX_KEY_SIZE + 3, 's');
  const std::string tag = expectedTag(input);
  std::string html;

  EXPECT_CALL(cfg, init()).WillOnce(Return(false));
  EXPECT_CALL(cfg, getInt32(StrEq(tag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 7;
        return true;
      });
  EXPECT_CALL(sender, send(_, _))
      .WillRepeatedly([&html](const char* data, int size) {
        html.append(data, size < 0 ? std::strlen(data)
                                   : static_cast<size_t>(size));
      });

  SelectInputParameterProbe param(input.c_str(), "label");
  param.registerValue("option", 7);
  param.send(&sender);

  EXPECT_THAT(html, ::testing::HasSubstr("name=\"" + tag + "\""));
  EXPECT_THAT(html, ::testing::HasSubstr("id=\"" + tag + "\""));
  EXPECT_THAT(html, ::testing::HasSubstr("for=\"" + tag + "\""));

  EXPECT_CALL(cfg, getInt32(StrEq(tag), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = 0;
        return true;
      });
  EXPECT_CALL(cfg, setInt32(StrEq(tag), 7)).Times(1);
  EXPECT_TRUE(param.handleResponse(tag.c_str(), "option"));
}
