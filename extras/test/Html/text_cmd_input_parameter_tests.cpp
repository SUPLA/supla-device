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
#include <supla/network/html/text_cmd_input_parameter.h>
#include <supla/network/web_sender.h>
#include <string>
#include <supla/actions.h>
#include <supla/events.h>

using ::testing::_;

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

class SenderMock : public Supla::WebSender {
 public:
  MOCK_METHOD(void, send, (const char*, int ), (override));
};

static std::string sendHtml;

TEST(TextCmdInputTests, EmptyElementTests) {
  Supla::Html::TextCmdInputParameter cmdInput("test_tag", "test_label");
  ActionHandlerMock ahMock;
  SenderMock senderMock;

  sendHtml.clear();

  EXPECT_EQ(sendHtml, "");

  EXPECT_CALL(senderMock, send(_, _)).WillRepeatedly([] (const char *data,
        int size) {
    sendHtml.append(data);
  });

  EXPECT_EQ(sendHtml, "");

  // Check generated HTML for TextCmdInput element
  cmdInput.send(&senderMock);
  EXPECT_EQ(sendHtml,
      "<div class=\"form-field\"><label for=\"test_tag\">test_label</label>"
      "<input type=\"text\"  name=\"test_tag\" id=\"test_tag\" ></div>");

  sendHtml.clear();

  // Check that no action is called on handler, when no command was registered
  const int actionId = 10;
  EXPECT_CALL(ahMock, handleAction(Supla::ON_EVENT_1, actionId)).Times(0);
  cmdInput.addAction(actionId, ahMock, Supla::ON_EVENT_1, true);

  cmdInput.handleResponse("test_tag", "testing cmd");
  cmdInput.handleResponse("test_tag", "");
  cmdInput.handleResponse("", "testing cmd");
}

TEST(TextCmdInputTests, CommandsTests) {
  Supla::Html::TextCmdInputParameter cmdInput("test_tag", "test_label");
  ActionHandlerMock ahMock1;
  ActionHandlerMock ahMock2;

  // There are 3 commands. "veni" generated EVENT_1, "vidi" and "vici"
  // generates EVENT_2.
  // There are 2 action handlers. First listen to EVENT_1 and EVENT_2, however
  // with different action ids.
  // Second handler listens to EVENT_2

  const int actionId = 10;
  const int actionId2 = 20;
  EXPECT_CALL(ahMock1, handleAction(Supla::ON_EVENT_1, actionId)).Times(1);
  EXPECT_CALL(ahMock1, handleAction(Supla::ON_EVENT_2, actionId2)).Times(2);
  EXPECT_CALL(ahMock2, handleAction(Supla::ON_EVENT_2, actionId)).Times(2);

  cmdInput.addAction(actionId, ahMock1, Supla::ON_EVENT_1, true);
  cmdInput.addAction(actionId2, ahMock1, Supla::ON_EVENT_2, true);
  cmdInput.addAction(actionId, ahMock2, Supla::ON_EVENT_2, true);

  cmdInput.registerCmd("veni", Supla::ON_EVENT_1);
  cmdInput.registerCmd("vidi", Supla::ON_EVENT_2);
  cmdInput.registerCmd("vici", Supla::ON_EVENT_2);

  cmdInput.handleResponse("test_tag", "veni");
  cmdInput.handleResponse("test_tag", "vidi");
  cmdInput.handleResponse("test_tag", "vici");

  // Calls with differen spelling of command or different tags should be ignored
  cmdInput.handleResponse("test_tag", "venius");
  cmdInput.handleResponse("test_tag", "VIDI");
  cmdInput.handleResponse("test_tag", "vic");
  cmdInput.handleResponse("test_tag", "");
  cmdInput.handleResponse("test_tag",
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      "this is very long text where each line is joined with previous one"
      );

  cmdInput.handleResponse("fredy", "veni");
  cmdInput.handleResponse("elon", "vidi");
  cmdInput.handleResponse("bill", "vici");
}

