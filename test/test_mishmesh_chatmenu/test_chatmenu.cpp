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

// Drives the embedded confirm dialog to its "Confirm" button and presses it.
static ChatMenu::Result confirm(ChatMenu& m, const char*& toast) {
  m.onInput(InputEvent::NavRight);   // Cancel (default) -> Confirm
  m.onInput(InputEvent::Select);
  return m.takeResult(toast);
}

TEST(ChatMenu, RegionRowRequestsEditor) {
  FakeMessagesService svc = twoChats();
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  EXPECT_EQ(0, m.selected());                       // Region is the first item
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::EditRegion, m.activate(&svc, toast));   // caller opens the keypad
  EXPECT_FALSE(m.modalActive());                     // no dialog armed for the region row
}

TEST(ChatMenu, ClearKeepsChatEmptiesMessages) {
  FakeMessagesService svc = twoChats();
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown);   // Region -> Notifications -> "Clear chat"
  EXPECT_EQ(2, m.selected());
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::None, m.activate(&svc, toast));   // arms the confirm dialog
  EXPECT_TRUE(m.modalActive());
  EXPECT_EQ(2, svc.convoCount());                   // nothing happened yet
  EXPECT_EQ(1, svc.messageCount(dm("ALICE!")));
  EXPECT_EQ(ChatMenu::Result::Cleared, confirm(m, toast));
  EXPECT_FALSE(m.modalActive());
  EXPECT_STREQ("Chat cleared", toast);
  EXPECT_EQ(2, svc.convoCount());                   // chat stays in the list
  EXPECT_EQ(0, svc.messageCount(dm("ALICE!")));
}

TEST(ChatMenu, CancelLeavesChatUntouched) {
  FakeMessagesService svc = twoChats();
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown);   // Region -> Notifications -> "Clear chat"
  const char* toast = nullptr;
  m.activate(&svc, toast);                          // Clear chat -> confirm dialog
  EXPECT_TRUE(m.modalActive());
  m.onInput(InputEvent::Back);                       // Back cancels the dialog
  EXPECT_FALSE(m.modalActive());
  EXPECT_EQ(ChatMenu::Result::None, m.takeResult(toast));
  EXPECT_EQ(2, svc.convoCount());                   // nothing cleared or deleted
  EXPECT_EQ(1, svc.messageCount(dm("ALICE!")));
}

TEST(ChatMenu, MarkUnreadFlagsChat) {
  FakeMessagesService svc = twoChats();
  svc.setActiveConvo(dm("ALICE!")); svc.clearActiveConvo();   // unread -> 0
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown);   // Region -> Notifications -> Clear -> "Mark unread"
  EXPECT_EQ(3, m.selected());
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::None, m.activate(&svc, toast));
  EXPECT_STREQ("Marked unread", toast);
  EXPECT_GT(svc.totalUnread(), 0u);
}

TEST(ChatMenu, DeleteRemovesChatFromList) {
  FakeMessagesService svc = twoChats();
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown);
  m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown);   // Region/Notifications/Clear/Mark -> "Delete chat"
  EXPECT_EQ(4, m.selected());
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::None, m.activate(&svc, toast));   // arms the confirm dialog
  EXPECT_TRUE(m.modalActive());
  EXPECT_EQ(2, svc.convoCount());                  // not gone yet
  EXPECT_EQ(ChatMenu::Result::Deleted, confirm(m, toast));
  EXPECT_STREQ("Chat deleted", toast);
  EXPECT_EQ(1, svc.convoCount());                  // ALICE gone, BOBBBB remains
  EXPECT_EQ(1, svc.messageCount(dm("BOBBBB")));
}

TEST(ChatMenu, NotificationsRowReturnsEditNotify) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k{}; k.type = 1; k.id[0] = 1;
  mishmesh::ChatMenu menu;
  menu.setTarget(k);
  menu.reset();
  menu.onInput(mishmesh::InputEvent::NavDown);   // row 0 Region -> row 1 Notifications
  const char* toast = nullptr;
  EXPECT_EQ(menu.activate(&svc, toast), mishmesh::ChatMenu::Result::EditNotify);
  EXPECT_FALSE(menu.modalActive());              // no stepper
}

static ConvoKey chan(uint8_t idx) { ConvoKey k{}; k.type = 1; k.id[0] = idx; return k; }

TEST(ChatMenu, ChannelExposesShareRow) {
  FakeMessagesService svc;
  ChatMenu m; m.setTarget(chan(1)); m.reset();
  m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown);   // Region -> Notifications -> Share
  EXPECT_EQ(2, m.selected());
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::Share, m.activate(&svc, toast));   // caller pushes the share screen
  EXPECT_FALSE(m.modalActive());                                  // Share arms no dialog
}

TEST(ChatMenu, DirectChatHasNoShareRow) {
  FakeMessagesService svc = twoChats();
  ChatMenu m; m.setTarget(dm("ALICE!")); m.reset();
  // Row 2 on a DM is "Clear chat", not Share (Share is channel-only).
  m.onInput(InputEvent::NavDown); m.onInput(InputEvent::NavDown);
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::None, m.activate(&svc, toast));    // Clear arms the dialog
  EXPECT_TRUE(m.modalActive());
}

TEST(ChatMenu, ChannelDeleteIndexShiftsPastShare) {
  FakeMessagesService svc;
  svc.store.appendInbound(chan(1), "hi", 2, 1, 1, 0, nullptr, 0);
  ChatMenu m; m.setTarget(chan(1)); m.reset();
  // Region, Notifications, Share, Clear, Mark unread, Delete -> Delete is index 5.
  for (int i = 0; i < 5; i++) m.onInput(InputEvent::NavDown);
  EXPECT_EQ(5, m.selected());
  const char* toast = nullptr;
  EXPECT_EQ(ChatMenu::Result::None, m.activate(&svc, toast));    // arms the confirm dialog
  EXPECT_TRUE(m.modalActive());
  EXPECT_EQ(1, svc.convoCount());
  EXPECT_EQ(ChatMenu::Result::Deleted, confirm(m, toast));
  EXPECT_STREQ("Chat deleted", toast);
  EXPECT_EQ(0, svc.convoCount());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
