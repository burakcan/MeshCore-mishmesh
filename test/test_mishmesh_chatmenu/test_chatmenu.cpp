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

// Notifications row arms the value stepper in place; stepping + Select writes the level.
TEST(ChatMenu, NotificationsRowStepsAndSetsLevel) {
  ChatMenu m;
  auto k = channelKey(2);
  m.setTarget(k);
  m.reset();
  m.onInput(InputEvent::NavDown);            // row 0 (Region) -> row 1 (Notifications)
  const char* toast = nullptr;
  FakeMessagesService svc;                   // starts at All (default)
  EXPECT_EQ(ChatMenu::Result::None, m.activate(&svc, toast));
  EXPECT_TRUE(m.modalActive());              // stepper is up
  m.onInput(InputEvent::NavRight);           // All -> Mentions only
  m.onInput(InputEvent::Select);             // confirm
  EXPECT_FALSE(m.modalActive());
  EXPECT_EQ(NotifyLevel::MentionsOnly, svc.notifyLevel(k));
}

// A DM's stepper offers only {All, Mute}: one NavRight from All lands on Mute.
TEST(ChatMenu, NotificationsRowDmStepsAllToMute) {
  ChatMenu m;
  auto k = dm("ALICE!");
  m.setTarget(k);
  m.reset();
  m.onInput(InputEvent::NavDown);            // -> Notifications
  const char* toast = nullptr;
  FakeMessagesService svc;
  m.activate(&svc, toast);
  m.onInput(InputEvent::NavRight);           // All -> Mute (no "Mentions only" on a DM)
  m.onInput(InputEvent::Select);
  EXPECT_EQ(NotifyLevel::Mute, svc.notifyLevel(k));
}

// Cancelling the stepper (Back) leaves the level unchanged.
TEST(ChatMenu, NotificationsStepperCancelLeavesLevel) {
  ChatMenu m;
  auto k = channelKey(2);
  m.setTarget(k);
  m.reset();
  m.onInput(InputEvent::NavDown);
  const char* toast = nullptr;
  FakeMessagesService svc;
  svc.setNotifyLevel(k, NotifyLevel::Mute);
  m.activate(&svc, toast);
  m.onInput(InputEvent::NavRight);           // would move, but...
  m.onInput(InputEvent::Back);               // ...cancel
  EXPECT_FALSE(m.modalActive());
  EXPECT_EQ(NotifyLevel::Mute, svc.notifyLevel(k));   // unchanged
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
