// test/test_mishmesh_messages/test_messages.cpp
#include <gtest/gtest.h>
#include "FakeMessagesService.h"
#include <mishmesh/applets/MessagesApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/applets/MessagePathApplet.h>
#include <mishmesh/core/AppletHost.h>
#include "FakeDisplayDriver.h"

TEST(MessagesService, FakeSmoke) {
  FakeMessagesService svc;
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "hi", 2, 1, 1, 0, nullptr, 0);
  EXPECT_EQ(1, svc.convoCount());
  mishmesh::ConvoView v; ASSERT_TRUE(svc.getConvo(0, v));
  EXPECT_EQ(1u, v.unread);  // unread populated (brief had inverted assertion)
}

TEST(MessagesService, GetMessageDelegatesToStore) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "hello", 5, 1000, 2000, -8, nullptr, 0);
  EXPECT_EQ(1, svc.messageCount(k));
  mishmesh::MessageView mv;
  ASSERT_TRUE(svc.getMessage(k, 0, mv));
  EXPECT_FALSE(mv.outbound);
  EXPECT_STREQ("hello", mv.text);
  EXPECT_EQ(-8, mv.snrx4);
}

TEST(MessagesService, TotalUnreadDelegatesToStore) {
  FakeMessagesService svc;
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  EXPECT_EQ(2u, svc.totalUnread());
}

TEST(MessagesService, DeleteMessageDelegatesToStore) {
  FakeMessagesService svc;
  mishmesh::ConvoKey k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "a", 1, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(k, "b", 1, 2, 2, 0, nullptr, 0);
  svc.deleteMessage(k, 0);
  EXPECT_EQ(1u, svc.deletes);
  EXPECT_EQ(1, svc.messageCount(k));
}

TEST(MessagesService, SeqIncrementsOnMutation) {
  FakeMessagesService svc;
  uint32_t s0 = svc.seq();
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "x", 1, 1, 1, 0, nullptr, 0);
  EXPECT_GT(svc.seq(), s0);
}

TEST(MessagesApplet, ListsConversations) {
  FakeMessagesService svc;
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "hi", 2, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(mishmesh::channelKey(3), "yo", 2, 2, 2, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.loop(0);
  // Chats tab default: 2 rows enumerated by the applet's list model
  EXPECT_EQ(2, mishmesh::messagesApplet().visibleRowCountForTest());
}

TEST(MessagesApplet, NewTabIsNoop) {
  FakeMessagesService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);  // -> New tab
  host.dispatch(mishmesh::InputEvent::Select);    // no-op + toast, must not crash
  SUCCEED();
}

// Long-pressing a chat row opens the chat-action overlay; "Delete chat" removes
// it from the list (which shrinks by one row).
TEST(MessagesApplet, LongPressDeletesChatFromList) {
  FakeMessagesService svc;
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"BOBBBB"), "b", 1, 2, 2, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.loop(0);
  EXPECT_EQ(2, mishmesh::messagesApplet().visibleRowCountForTest());
  host.dispatch(mishmesh::InputEvent::SelectLong);   // open overlay on selected row
  EXPECT_TRUE(mishmesh::messagesApplet().menuOpenForTest());
  host.dispatch(mishmesh::InputEvent::NavDown);       // -> Mark unread
  host.dispatch(mishmesh::InputEvent::NavDown);       // -> Delete chat
  host.dispatch(mishmesh::InputEvent::Select);        // invoke Delete
  EXPECT_FALSE(mishmesh::messagesApplet().menuOpenForTest());
  EXPECT_EQ(1, svc.convoCount());
  EXPECT_EQ(1, mishmesh::messagesApplet().visibleRowCountForTest());
}

// Back closes the overlay without acting.
TEST(MessagesApplet, LongPressMenuBackCloses) {
  FakeMessagesService svc;
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.loop(0);
  host.dispatch(mishmesh::InputEvent::SelectLong);
  EXPECT_TRUE(mishmesh::messagesApplet().menuOpenForTest());
  host.dispatch(mishmesh::InputEvent::Back);
  EXPECT_FALSE(mishmesh::messagesApplet().menuOpenForTest());
  EXPECT_EQ(1, svc.convoCount());   // nothing deleted
}

TEST(MessageThread, OpensFocusedOnNewestAndClearsUnread) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "one", 3, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(k, "two", 3, 2, 2, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.loop(0);
  EXPECT_EQ(0u, svc.totalUnread());                       // opening cleared unread
  EXPECT_EQ(1, mishmesh::messageThreadApplet().focusedIndexForTest());  // newest (index 1) focused
}

TEST(MessageThread, NavUpMovesFocusOlder) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "one", 3, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(k, "two", 3, 2, 2, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::NavUp);
  EXPECT_EQ(0, mishmesh::messageThreadApplet().focusedIndexForTest());
}

// Both messages are KIND_INBOUND so hasPath=true -> 3-item menu: Reply(0)/Delete(1)/ViewPath(2).
// Menu opens at index 0; one NavDown lands on Delete (index 1); Select invokes it.
TEST(MessageThread, SelectDeletesViaPerMessageMenu) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "one", 3, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(k, "two", 3, 2, 2, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::Select);    // open per-message menu (focus=newest)
  host.dispatch(mishmesh::InputEvent::NavDown);   // move to "Delete" (index 1)
  host.dispatch(mishmesh::InputEvent::Select);    // invoke Delete
  EXPECT_EQ(1u, svc.deletes);
  EXPECT_EQ(1, svc.messageCount(k));
}

// Settings tab: NavRight switches to it; selecting "Clear chat" empties the
// conversation but keeps us in the thread, switched back to the conversation tab.
TEST(MessageThread, SettingsTabClearKeepsThread) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "one", 3, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(k, "two", 3, 2, 2, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::NavRight);   // -> Settings tab
  EXPECT_EQ(1, mishmesh::messageThreadApplet().selectedTabForTest());
  host.dispatch(mishmesh::InputEvent::Select);     // "Clear chat" (first item)
  EXPECT_EQ(0, svc.messageCount(k));               // emptied
  EXPECT_EQ(1, svc.convoCount());                  // chat still exists
  EXPECT_EQ(d0, host.depth());                     // still in the thread
  EXPECT_EQ(0, mishmesh::messageThreadApplet().selectedTabForTest());  // back on conversation
}

// Selecting "Delete chat" removes the conversation and pops back to the list.
TEST(MessageThread, SettingsTabDeletePopsToList) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "one", 3, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::NavRight);   // -> Settings tab
  host.dispatch(mishmesh::InputEvent::NavDown);    // -> Mark unread
  host.dispatch(mishmesh::InputEvent::NavDown);    // -> Delete chat
  host.dispatch(mishmesh::InputEvent::Select);     // invoke Delete
  EXPECT_EQ(0, svc.convoCount());                  // chat removed
  EXPECT_EQ(d0 - 1, host.depth());                 // popped back to the list
}

TEST(MessagePath, InboundShowsHops) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  uint8_t path[3] = {0x5B, 0x63, 0x5E};
  svc.store.appendInbound(k, "hi", 2, 1, 1, -32, path, 3);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messagePathApplet().setTarget(k, 0);
  host.push(&mishmesh::messagePathApplet());
  host.loop(0);
  EXPECT_EQ(3, mishmesh::messagePathApplet().rowCountForTest());
}

TEST(MessagePath, ChannelShowsRepeats) {
  FakeMessagesService svc;
  auto k = mishmesh::channelKey(2);
  svc.store.appendOutboundChannel(k, "x", 1, 9, 9);
  uint8_t p[1] = {0x33};
  svc.store.addRepeat(k, 9, 7, p, 1);
  svc.store.addRepeat(k, 9, -1, p, 1);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messagePathApplet().setTarget(k, 0);
  host.push(&mishmesh::messagePathApplet());
  host.loop(0);
  EXPECT_EQ(2, mishmesh::messagePathApplet().rowCountForTest());
}

TEST(MessagesBadge, TotalUnreadReflectsInbound) {
  FakeMessagesService svc;
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(mishmesh::channelKey(3), "b", 1, 2, 2, 0, nullptr, 0);
  EXPECT_EQ(2u, svc.totalUnread());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
