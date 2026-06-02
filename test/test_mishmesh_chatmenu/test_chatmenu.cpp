#include <gtest/gtest.h>
#include <mishmesh/widgets/ChatMenu.h>
#include "FakeMessagesService.h"
using namespace mishmesh;

static ConvoKey dm(const char* p) { return directKey((const uint8_t*)p); }

static FakeMessagesService twoChats() {
  FakeMessagesService svc;
  svc.store.appendInbound(dm("ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(dm("BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  return svc;
}

TEST(ChatMenu, ClearKeepsChatEmptiesMessages) {
  FakeMessagesService svc = twoChats();
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  EXPECT_EQ(0, m.selected());                       // Clear chat is the first item
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::Cleared, m.activate(&svc, toast));
  EXPECT_STREQ("Chat cleared", toast);
  EXPECT_EQ(2, svc.convoCount());                   // chat stays in the list
  EXPECT_EQ(0, svc.messageCount(dm("ALICE!")));
}

TEST(ChatMenu, MarkUnreadFlagsChat) {
  FakeMessagesService svc = twoChats();
  svc.setActiveConvo(dm("ALICE!")); svc.clearActiveConvo();   // unread -> 0
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  m.onInput(InputEvent::NavDown);                  // -> "Mark unread"
  EXPECT_EQ(1, m.selected());
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::None, m.activate(&svc, toast));
  EXPECT_STREQ("Marked unread", toast);
  EXPECT_GT(svc.totalUnread(), 0u);
}

TEST(ChatMenu, DeleteRemovesChatFromList) {
  FakeMessagesService svc = twoChats();
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown);   // -> "Delete chat"
  EXPECT_EQ(2, m.selected());
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::Deleted, m.activate(&svc, toast));
  EXPECT_STREQ("Chat deleted", toast);
  EXPECT_EQ(1, svc.convoCount());                  // ALICE gone, BOBBBB remains
  EXPECT_EQ(1, svc.messageCount(dm("BOBBBB")));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
