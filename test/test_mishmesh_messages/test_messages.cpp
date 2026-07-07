// test/test_mishmesh_messages/test_messages.cpp
#include <gtest/gtest.h>
#include "FakeMessagesService.h"
#include "FakeContactsService.h"
#include <mishmesh/applets/MessagesApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/applets/MessagePathApplet.h>
#include <mishmesh/applets/ContactsApplet.h>
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

// The Settings tab (Chats -> New -> Settings) lists the three rows and writes
// each setting back through the service.
TEST(MessagesApplet, SettingsTabTogglesAndAcks) {
  FakeMessagesService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.loop(0);
  host.dispatch(mishmesh::InputEvent::NavRight);   // Chats -> New
  host.dispatch(mishmesh::InputEvent::NavRight);   // New -> Settings
  EXPECT_EQ(2, mishmesh::messagesApplet().selectedTabForTest());
  EXPECT_EQ(3, mishmesh::messagesApplet().visibleRowCountForTest());

  // Row 0: Auto retry -> toggle on
  EXPECT_FALSE(svc.getMessagesConfig().autoRetry);
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_TRUE(svc.getMessagesConfig().autoRetry);

  // Row 1: Auto reset path -> toggle on
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_TRUE(svc.getMessagesConfig().autoResetPath);

  // Row 2: Direct msg acks -> carousel 1 -> 2 -> confirm
  EXPECT_EQ(1, svc.getMessagesConfig().directAcks);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);     // open the stepper
  host.dispatch(mishmesh::InputEvent::NavRight);   // 1 -> 2
  host.dispatch(mishmesh::InputEvent::Select);     // confirm
  EXPECT_EQ(2, svc.getMessagesConfig().directAcks);
}

// Dismissing the acks carousel with Back leaves the value unchanged.
TEST(MessagesApplet, SettingsAcksCancelKeepsValue) {
  FakeMessagesService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.loop(0);
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.dispatch(mishmesh::InputEvent::NavDown);     // -> Auto reset path
  host.dispatch(mishmesh::InputEvent::NavDown);     // -> Direct msg acks
  host.dispatch(mishmesh::InputEvent::Select);      // open stepper
  host.dispatch(mishmesh::InputEvent::NavRight);    // 1 -> 2 (uncommitted)
  host.dispatch(mishmesh::InputEvent::Back);        // cancel
  EXPECT_EQ(1, svc.getMessagesConfig().directAcks); // unchanged
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
  host.dispatch(mishmesh::InputEvent::NavDown);       // Region -> Notifications
  host.dispatch(mishmesh::InputEvent::NavDown);       // -> Clear chat
  host.dispatch(mishmesh::InputEvent::NavDown);       // -> Mark unread
  host.dispatch(mishmesh::InputEvent::NavDown);       // -> Delete chat
  host.dispatch(mishmesh::InputEvent::Select);        // arm the confirm dialog
  EXPECT_TRUE(mishmesh::messagesApplet().menuOpenForTest());   // still open, awaiting confirm
  EXPECT_EQ(2, svc.convoCount());                     // nothing deleted yet
  host.dispatch(mishmesh::InputEvent::NavRight);      // Cancel -> Confirm
  host.dispatch(mishmesh::InputEvent::Select);        // confirm Delete
  EXPECT_FALSE(mishmesh::messagesApplet().menuOpenForTest());
  EXPECT_EQ(1, svc.convoCount());
  EXPECT_EQ(1, mishmesh::messagesApplet().visibleRowCountForTest());
}

// Selecting the Region row launches the keypad editor over the still-open overlay.
TEST(MessagesApplet, LongPressRegionOpensEditor) {
  FakeMessagesService svc;
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.loop(0);
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::SelectLong);   // open overlay (Region row selected)
  EXPECT_TRUE(mishmesh::messagesApplet().menuOpenForTest());
  host.dispatch(mishmesh::InputEvent::Select);       // Region -> launch keypad editor
  EXPECT_EQ(d0 + 1, host.depth());                   // keypad pushed on top
  EXPECT_TRUE(mishmesh::messagesApplet().menuOpenForTest());   // overlay stays open underneath
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
  host.dispatch(mishmesh::InputEvent::NavDown);    // Region -> Notifications
  host.dispatch(mishmesh::InputEvent::NavDown);    // -> "Clear chat"
  host.dispatch(mishmesh::InputEvent::Select);     // "Clear chat" -> confirm dialog
  EXPECT_EQ(2, svc.messageCount(k));               // not emptied yet
  EXPECT_EQ(1, mishmesh::messageThreadApplet().selectedTabForTest());  // still on Settings tab
  host.dispatch(mishmesh::InputEvent::NavRight);   // Cancel -> Confirm
  host.dispatch(mishmesh::InputEvent::Select);     // confirm Clear
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
  host.dispatch(mishmesh::InputEvent::NavDown);    // Region -> Notifications
  host.dispatch(mishmesh::InputEvent::NavDown);    // -> Clear chat
  host.dispatch(mishmesh::InputEvent::NavDown);    // -> Mark unread
  host.dispatch(mishmesh::InputEvent::NavDown);    // -> Delete chat
  host.dispatch(mishmesh::InputEvent::Select);     // arm the confirm dialog
  EXPECT_EQ(1, svc.convoCount());                  // not removed yet
  EXPECT_EQ(d0, host.depth());                     // still in the thread
  host.dispatch(mishmesh::InputEvent::NavRight);   // Cancel -> Confirm
  host.dispatch(mishmesh::InputEvent::Select);     // confirm Delete
  EXPECT_EQ(0, svc.convoCount());                  // chat removed
  EXPECT_EQ(d0 - 1, host.depth());                 // popped back to the list
}

TEST(MessageThread, OutboundChannelMenuSaysHeardRepeats) {
  FakeMessagesService svc;
  auto k = mishmesh::channelKey(1);
  svc.store.appendOutboundChannel(k, "hey", 3, 5, 5);
  svc.store.addRepeat(k, 5, -8, nullptr, 0);   // heard once -> no "Resend" offered
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::Select);   // open per-message menu on the message
  EXPECT_STREQ("Reply", mishmesh::messageThreadApplet().msgMenuLabelForTest(0));
  EXPECT_STREQ("Heard Repeats", mishmesh::messageThreadApplet().msgMenuLabelForTest(2));
}

// A channel message nobody has echoed back (0 heards) offers a manual Resend,
// which re-sends the body as a fresh outbound message.
TEST(MessageThread, ZeroHeardChannelOffersResend) {
  FakeMessagesService svc;
  auto k = mishmesh::channelKey(1);
  svc.store.appendOutboundChannel(k, "hey", 3, 5, 5);   // heardCount 0
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::Select);          // open per-message menu
  EXPECT_STREQ("Resend", mishmesh::messageThreadApplet().msgMenuLabelForTest(1));
  EXPECT_STREQ("Heard Repeats", mishmesh::messageThreadApplet().msgMenuLabelForTest(3));
  host.dispatch(mishmesh::InputEvent::NavDown);          // Reply -> Resend
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(1, svc.sends);
  EXPECT_EQ("hey", svc.lastSent);
}

// A direct message that exhausted its retries (ST_FAILED) offers a manual Resend.
TEST(MessageThread, FailedDMOffersResend) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendOutboundDM(k, "yo", 2, 9, 9, 0xABCD, 0);
  svc.store.markFailed(k, 9);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::Select);          // open per-message menu (DM: no path row)
  EXPECT_STREQ("Resend", mishmesh::messageThreadApplet().msgMenuLabelForTest(1));
  host.dispatch(mishmesh::InputEvent::NavDown);          // Reply -> Resend
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(1, svc.sends);
  EXPECT_EQ("yo", svc.lastSent);
}

// A still-pending (not failed) DM does NOT offer Resend - auto-retry owns it.
TEST(MessageThread, PendingDMHasNoResend) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendOutboundDM(k, "yo", 2, 9, 9, 0xABCD, 0);   // ST_PENDING
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_STREQ("Reply", mishmesh::messageThreadApplet().msgMenuLabelForTest(0));
  EXPECT_STREQ("Delete", mishmesh::messageThreadApplet().msgMenuLabelForTest(1));  // no Resend between
}

TEST(MessageThread, InboundMenuSaysViewPath) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "hi", 2, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_STREQ("View Path", mishmesh::messageThreadApplet().msgMenuLabelForTest(2));
}

TEST(MessagePath, InboundResolvesNameElseHex) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  uint8_t path[2] = {0xAA, 0x5B};            // 0xAA is the only byte the fake resolves
  svc.store.appendInbound(k, "hi", 2, 1, 1, -30, path, 2);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messagePathApplet().setTarget(k, 0);
  host.push(&mishmesh::messagePathApplet());
  host.loop(0);
  EXPECT_STREQ("Path: 2 hops", mishmesh::messagePathApplet().titleForTest());
  EXPECT_EQ(2, mishmesh::messagePathApplet().rowCountForTest());
  EXPECT_STREQ("1  Alice", mishmesh::messagePathApplet().lineForTest(0));
  EXPECT_STREQ("2  5B",    mishmesh::messagePathApplet().lineForTest(1));
}

TEST(MessagePath, InboundDirectHasNoHops) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "hi", 2, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messagePathApplet().setTarget(k, 0);
  host.push(&mishmesh::messagePathApplet());
  host.loop(0);
  EXPECT_STREQ("Direct", mishmesh::messagePathApplet().titleForTest());
  EXPECT_EQ(0, mishmesh::messagePathApplet().rowCountForTest());
  EXPECT_STREQ("Direct (no path)", mishmesh::messagePathApplet().lineForTest(0));
}

// Each re-hearing shows the repeater we actually heard THAT echo from - the last
// hop (path[pathLen-1]), not the origin-side path[0] (which is always our nearest
// repeater and thus identical across echoes).
TEST(MessagePath, ChannelShowsLastHeardRepeater) {
  FakeMessagesService svc;
  auto k = mishmesh::channelKey(2);
  svc.store.appendOutboundChannel(k, "x", 1, 9, 9);
  uint8_t p[2] = {0xAA, 0x5B};               // path: Alice -> 5B
  svc.store.addRepeat(k, 9, 7, p, 2);        // heard via 5B (last hop)
  svc.store.addRepeat(k, 9, -1, p, 1);       // heard directly from Alice
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messagePathApplet().setTarget(k, 0);
  host.push(&mishmesh::messagePathApplet());
  host.loop(0);
  EXPECT_STREQ("Heard 2 times", mishmesh::messagePathApplet().titleForTest());
  EXPECT_EQ(2, mishmesh::messagePathApplet().rowCountForTest());     // counts re-hearings
  EXPECT_STREQ("#1   2h | 1.8dB",  mishmesh::messagePathApplet().lineForTest(0));
  EXPECT_STREQ("  5B",             mishmesh::messagePathApplet().lineForTest(1));
  EXPECT_STREQ("#2   1h | -0.2dB", mishmesh::messagePathApplet().lineForTest(2));
  EXPECT_STREQ("  Alice",          mishmesh::messagePathApplet().lineForTest(3));
}

// Outbound channel with no repeats yet (heardCount==0) -> "Not heard yet".
// The sibling "Details not available" branch (repeatCount()==0 but heardCount>0,
// i.e. per-repeat detail lost on reboot while the persisted heard counter survives)
// is not reachable from the public store API in a host test: addRepeat() always
// adds a Tracked repeat AND bumps heardCount together, so it cannot be reproduced
// without reboot/persistence state.
TEST(MessagePath, ChannelNotHeardYet) {
  FakeMessagesService svc;
  auto k = mishmesh::channelKey(2);
  svc.store.appendOutboundChannel(k, "x", 1, 9, 9);   // no repeats added
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messagePathApplet().setTarget(k, 0);
  host.push(&mishmesh::messagePathApplet());
  host.loop(0);
  EXPECT_STREQ("Heard repeats", mishmesh::messagePathApplet().titleForTest());
  EXPECT_EQ(0, mishmesh::messagePathApplet().rowCountForTest());
  EXPECT_STREQ("Not heard yet", mishmesh::messagePathApplet().lineForTest(0));
}

TEST(MessagesBadge, TotalUnreadReflectsInbound) {
  FakeMessagesService svc;
  svc.store.appendInbound(mishmesh::directKey((const uint8_t*)"ALICE!"), "a", 1, 1, 1, 0, nullptr, 0);
  svc.store.appendInbound(mishmesh::channelKey(3), "b", 1, 2, 2, 0, nullptr, 0);
  EXPECT_EQ(2u, svc.totalUnread());
}

TEST(MessagesService, SendTextRecordsAndAppends) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  EXPECT_TRUE(svc.sendText(k, "hello"));
  EXPECT_EQ(1, svc.sends);
  EXPECT_EQ("hello", svc.lastSent);
  EXPECT_EQ(1, svc.messageCount(k));      // appended as an outbound message
  mishmesh::MessageView mv;
  ASSERT_TRUE(svc.getMessage(k, 0, mv));
  EXPECT_TRUE(mv.outbound);
  EXPECT_STREQ("hello", mv.text);
}

// NavDown past the newest message focuses the button bar (Write = row 0).
TEST(MessageThread, NavDownPastLastEntersButtonBar) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "one", 3, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  EXPECT_EQ(-1, mishmesh::messageThreadApplet().barRowForTest());  // starts on a message
  host.dispatch(mishmesh::InputEvent::NavDown);                    // only message -> bar
  EXPECT_EQ(0, mishmesh::messageThreadApplet().barRowForTest());   // Write focused
  host.dispatch(mishmesh::InputEvent::NavDown);                    // -> Quick
  EXPECT_EQ(1, mishmesh::messageThreadApplet().barRowForTest());
  host.dispatch(mishmesh::InputEvent::NavUp);                      // -> Write
  EXPECT_EQ(0, mishmesh::messageThreadApplet().barRowForTest());
  host.dispatch(mishmesh::InputEvent::NavUp);                      // -> back to message
  EXPECT_EQ(-1, mishmesh::messageThreadApplet().barRowForTest());
}

// Write button opens the keypad; typing + OK sends via sendText and pops back.
TEST(MessageThread, WriteButtonComposesAndSends) {
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
  host.dispatch(mishmesh::InputEvent::NavDown);    // focus Write
  host.dispatch(mishmesh::InputEvent::Select);     // open keypad
  EXPECT_EQ(d0 + 1, host.depth());
  host.dispatch(mishmesh::InputEvent::Select);     // type 'a' (keypad opens on "abc")
  mishmesh::keypadApplet().setFocusForTest(3, 3);  // OK cell
  host.dispatch(mishmesh::InputEvent::Select);     // confirm -> sendText + pop
  EXPECT_EQ(d0, host.depth());                     // back in the thread
  EXPECT_EQ(1, svc.sends);
  EXPECT_EQ("a", svc.lastSent);
  EXPECT_TRUE(svc.lastSentKey.equals(k));
}

// Quick button is a no-op for now (does not send, does not crash).
TEST(MessageThread, QuickButtonIsNoop) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "one", 3, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::NavDown);    // Write
  host.dispatch(mishmesh::InputEvent::NavDown);    // Quick
  host.dispatch(mishmesh::InputEvent::Select);     // no-op toast
  EXPECT_EQ(0, svc.sends);
  SUCCEED();
}

// A thread opened for a contact with no conversation yet still shows the name.
TEST(MessageThread, FallbackNameShownWhenNoConvo) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ZEBRA!");   // no messages for this key
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k, "Zara");
  host.push(&mishmesh::messageThreadApplet());
  EXPECT_STREQ("Zara", mishmesh::messageThreadApplet().headerTitleForTest());
}

// Reply on a DM opens compose with an empty buffer.
TEST(MessageThread, ReplyOnDmOpensEmptyCompose) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "hi", 2, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);    // open per-message menu (Reply=0 selected)
  host.dispatch(mishmesh::InputEvent::Select);    // invoke Reply
  EXPECT_EQ(d0 + 1, host.depth());                // keypad pushed
  EXPECT_STREQ("", mishmesh::keypadApplet().text());
}

// Reply on a channel message seeds "@[<sender>] " (the app's mention format) into compose.
TEST(MessageThread, ReplyOnChannelSeedsMention) {
  FakeMessagesService svc;
  svc.senderName = "Bob";
  auto k = mishmesh::channelKey(2);
  svc.store.appendInbound(k, "yo", 2, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.dispatch(mishmesh::InputEvent::Select);    // open per-message menu
  host.dispatch(mishmesh::InputEvent::Select);    // invoke Reply (index 0)
  EXPECT_STREQ("@[Bob] ", mishmesh::keypadApplet().text());
}

// "New message" (New tab, row 0) opens the contacts picker in pick mode.
TEST(MessagesApplet, NewMessageOpensPicker) {
  FakeMessagesService svc;
  FakeContactsService contacts;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc; ctx.contacts = &contacts;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);  // -> New tab
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);    // row 0 = "New message"
  EXPECT_EQ(d0 + 1, host.depth());
  EXPECT_TRUE(mishmesh::contactsApplet().pickModeForTest());
}

// Row 1 is now "Create private channel" (was: "New group" noop); must not crash.
TEST(MessagesApplet, NewGroupIsNoop) {
  FakeMessagesService svc;
  FakeContactsService contacts;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc; ctx.contacts = &contacts;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);  // -> New tab
  host.dispatch(mishmesh::InputEvent::NavDown);   // -> "Create private channel" (row 1)
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);    // opens the Create-private form
  EXPECT_EQ(d0 + 1, host.depth());                // form was pushed
}

// Opening a thread with composeOnOpen() focuses the Write button, not the last message.
TEST(MessageThread, ComposeOnOpenFocusesWriteButton) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "hi", 2, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  mishmesh::messageThreadApplet().composeOnOpen();
  host.push(&mishmesh::messageThreadApplet());
  EXPECT_EQ(0, mishmesh::messageThreadApplet().barRowForTest());   // Write button focused
}

// A normal open (no composeOnOpen) still focuses the last message - and confirms
// the composeOnOpen one-shot was consumed by the prior open of the shared singleton.
TEST(MessageThread, NormalOpenFocusesLastMessage) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  svc.store.appendInbound(k, "hi", 2, 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  EXPECT_EQ(-1, mishmesh::messageThreadApplet().barRowForTest());  // on the message
}

// Empty thread: the Write/Quick buttons pin to the bottom, clear of the
// "No messages" hint (regression: they used to render at the top, overlapping).
TEST(MessageThread, EmptyThreadButtonsBottomPinned) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"EMPTY!");   // no messages for this key
  FakeDisplayDriver d;                                       // 128x64
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  d.rects.clear();
  host.loop(0);                                              // render the empty thread

  // Two full-width button borders (w==124, h==16). Both must sit in the bottom
  // band: device y >= HDR_H(13) + bodyH(51) - BTN_H(36) = 28, never over the hint.
  int btnBorders = 0;
  for (auto& r : d.rects) if (r.w == 124 && r.h == 16) {
    btnBorders++;
    EXPECT_GE(r.y, 28) << "action bar button drawn too high (overlaps the hint)";
  }
  EXPECT_EQ(2, btnBorders);
}

// Empty thread: the action bar stays reachable. NavDown lands on Write, moves to
// Quick, NavUp returns to Write, and never strands on phantom message focus (-1).
TEST(MessageThread, EmptyThreadBarStaysReachable) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"EMPTY!");   // no messages
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  EXPECT_EQ(-1, mishmesh::messageThreadApplet().barRowForTest());   // opens unfocused
  host.dispatch(mishmesh::InputEvent::NavDown);
  EXPECT_EQ(0, mishmesh::messageThreadApplet().barRowForTest());    // Write
  host.dispatch(mishmesh::InputEvent::NavDown);
  EXPECT_EQ(1, mishmesh::messageThreadApplet().barRowForTest());    // Quick
  host.dispatch(mishmesh::InputEvent::NavUp);
  EXPECT_EQ(0, mishmesh::messageThreadApplet().barRowForTest());    // back to Write
  host.dispatch(mishmesh::InputEvent::NavUp);
  EXPECT_EQ(0, mishmesh::messageThreadApplet().barRowForTest());    // stays on Write, not stranded
}

// composeOnOpen on an empty thread auto-focuses Write; NavUp must keep it there
// (previously it dropped to -1 and the bar became unreachable).
TEST(MessageThread, EmptyThreadComposeOpenNavUpKeepsWrite) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"EMPTY!");
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  mishmesh::messageThreadApplet().composeOnOpen();
  host.push(&mishmesh::messageThreadApplet());
  EXPECT_EQ(0, mishmesh::messageThreadApplet().barRowForTest());    // auto-focused Write
  host.dispatch(mishmesh::InputEvent::NavUp);
  EXPECT_EQ(0, mishmesh::messageThreadApplet().barRowForTest());    // still Write
}

// ensureChannel seeds an empty chat for a joined channel so it shows before any
// message arrives (e.g. the default Public channel on a fresh device).
TEST(MessageStore, EnsureChannelSeedsEmptyConvo) {
  mishmesh::MessageStore store;
  EXPECT_EQ(0, store.convoCount());
  store.ensureChannel(0);
  ASSERT_EQ(1, store.convoCount());
  mishmesh::ConvoSummary c;
  ASSERT_TRUE(store.getConvo(0, c));
  EXPECT_EQ(1, c.key.type);            // channel
  EXPECT_EQ(0, c.key.id[0]);           // channel index 0
  EXPECT_EQ(0, store.messageCount(mishmesh::channelKey(0)));   // no messages yet
}

// Idempotent: re-seeding an already-known channel (e.g. one with messages) is a no-op.
TEST(MessageStore, EnsureChannelIdempotent) {
  mishmesh::MessageStore store;
  store.appendInbound(mishmesh::channelKey(2), "yo", 2, 5, 5, 0, nullptr, 0);
  EXPECT_EQ(1, store.convoCount());
  store.ensureChannel(2);              // already present via the inbound message
  EXPECT_EQ(1, store.convoCount());    // not duplicated
  EXPECT_EQ(1, store.messageCount(mishmesh::channelKey(2)));   // message preserved
}

TEST(MessagesService, ChannelOpsRecordedByFake) {
  FakeMessagesService svc;
  svc.chanResult = mishmesh::ChanResult::Full;
  EXPECT_EQ(mishmesh::ChanResult::Full, svc.createPrivateChannel("teamA"));
  EXPECT_EQ(FakeMessagesService::OP_CREATE_PRIV, svc.lastChanOp);
  EXPECT_EQ("teamA", svc.lastChanName);

  svc.chanResult = mishmesh::ChanResult::Ok;
  EXPECT_EQ(mishmesh::ChanResult::Ok, svc.joinPrivateChannel("p", "00112233445566778899aabbccddeeff"));
  EXPECT_EQ(FakeMessagesService::OP_JOIN_PRIV, svc.lastChanOp);
  EXPECT_EQ("p", svc.lastChanName);
  EXPECT_EQ("00112233445566778899aabbccddeeff", svc.lastChanKey);

  EXPECT_EQ(mishmesh::ChanResult::Ok, svc.joinPublicChannel());
  EXPECT_EQ(FakeMessagesService::OP_JOIN_PUB, svc.lastChanOp);

  svc.joinHashtagChannel("#trending");
  EXPECT_EQ(FakeMessagesService::OP_JOIN_HASH, svc.lastChanOp);
  EXPECT_EQ("#trending", svc.lastChanName);

  svc.publicJoined = true;
  EXPECT_TRUE(svc.publicChannelJoined());
  EXPECT_EQ(4, svc.chanCalls);
}

// openNewTab: destroy-and-recreate the host each call so every test gets a
// fresh stack + fresh ctx (setRoot requires depth==0; static svc pointers
// would dangle across test boundaries). new/delete is fine in host tests.
static mishmesh::AppletHost* gNewTabHost = nullptr;

static mishmesh::AppletHost* openNewTab(FakeMessagesService& svc, FakeDisplayDriver& d) {
  delete gNewTabHost;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  gNewTabHost = new mishmesh::AppletHost(&d, ctx);
  gNewTabHost->setRoot(&mishmesh::messagesApplet());
  gNewTabHost->dispatch(mishmesh::InputEvent::NavRight);   // Chats -> New
  gNewTabHost->loop(0);
  return gNewTabHost;
}

TEST(MessagesApplet, NewTabHidesPublicWhenJoined) {
  FakeMessagesService svc; svc.publicJoined = true;
  FakeDisplayDriver d; openNewTab(svc, d);
  EXPECT_EQ(4, mishmesh::messagesApplet().visibleRowCountForTest());
}

TEST(MessagesApplet, NewTabShowsPublicWhenNotJoined) {
  FakeMessagesService svc; svc.publicJoined = false;
  FakeDisplayDriver d; openNewTab(svc, d);
  EXPECT_EQ(5, mishmesh::messagesApplet().visibleRowCountForTest());
}

TEST(MessagesApplet, JoinPublicCallsServiceNoForm) {
  FakeMessagesService svc; svc.publicJoined = false; svc.chanResult = mishmesh::ChanResult::Ok;
  FakeDisplayDriver d; auto* host = openNewTab(svc, d);
  // rows: 0 msg,1 create,2 join-priv,3 join-public,4 hashtag
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);   // -> row 3 (Join public)
  host->dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(FakeMessagesService::OP_JOIN_PUB, svc.lastChanOp);
  EXPECT_EQ(1, host->depth());                      // no form pushed
  EXPECT_EQ(0, mishmesh::messagesApplet().selectedTabForTest());  // switched to Chats on Ok
}

TEST(MessagesApplet, CreatePrivateOpensKeypadAndCreates) {
  FakeMessagesService svc; svc.publicJoined = true; svc.chanResult = mishmesh::ChanResult::Ok;
  FakeDisplayDriver d; auto* host = openNewTab(svc, d);
  host->dispatch(mishmesh::InputEvent::NavDown);    // -> row 1 (Create private)
  host->dispatch(mishmesh::InputEvent::Select);     // push keypad (not a form)
  EXPECT_EQ(2, host->depth());
  host->dispatch(mishmesh::InputEvent::Select);     // type 'a' (keypad opens on the abc cell)
  mishmesh::keypadApplet().setFocusForTest(3, 3);   // OK cell
  host->dispatch(mishmesh::InputEvent::Select);     // confirm -> onCreatePrivateDone
  EXPECT_EQ(FakeMessagesService::OP_CREATE_PRIV, svc.lastChanOp);
  EXPECT_EQ("a", svc.lastChanName);
  EXPECT_EQ(1, host->depth());                       // keypad popped
  EXPECT_EQ(0, mishmesh::messagesApplet().selectedTabForTest());   // switched to Chats on Ok
}

TEST(MessagesApplet, JoinPrivateRejectsBadKey) {
  FakeMessagesService svc; svc.publicJoined = true;
  FakeDisplayDriver d; auto* host = openNewTab(svc, d);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);    // -> row 2 (Join private)
  host->dispatch(mishmesh::InputEvent::Select);     // push JoinPrivateApplet
  EXPECT_EQ(2, host->depth());
  mishmesh::messagesApplet().setChannelNameForTest("p");
  mishmesh::messagesApplet().setChannelKeyForTest("nothex");   // invalid
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);     // focus Join button
  host->dispatch(mishmesh::InputEvent::Select);      // blocked by isHexKey
  EXPECT_EQ(0, svc.chanCalls);
  EXPECT_EQ(2, host->depth());                        // stays open
}

TEST(MessagesApplet, JoinPrivateHappyPath) {
  FakeMessagesService svc; svc.publicJoined = true; svc.chanResult = mishmesh::ChanResult::Ok;
  FakeDisplayDriver d; auto* host = openNewTab(svc, d);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::Select);       // push JoinPrivateApplet
  mishmesh::messagesApplet().setChannelNameForTest("p");
  mishmesh::messagesApplet().setChannelKeyForTest("00112233445566778899aabbccddeeff");
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);       // focus Join
  host->dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(FakeMessagesService::OP_JOIN_PRIV, svc.lastChanOp);
  EXPECT_EQ("00112233445566778899aabbccddeeff", svc.lastChanKey);
  EXPECT_EQ(1, host->depth());                          // popped on Ok
}

TEST(MessagesApplet, CreatePrivateFullPopsKeypadWithToast) {
  FakeMessagesService svc; svc.publicJoined = true; svc.chanResult = mishmesh::ChanResult::Full;
  FakeDisplayDriver d; auto* host = openNewTab(svc, d);
  host->dispatch(mishmesh::InputEvent::NavDown);    // -> row 1 (Create private)
  host->dispatch(mishmesh::InputEvent::Select);     // push keypad
  host->dispatch(mishmesh::InputEvent::Select);     // type a char
  mishmesh::keypadApplet().setFocusForTest(3, 3);   // OK cell
  host->dispatch(mishmesh::InputEvent::Select);     // confirm -> createPrivate returns Full
  EXPECT_EQ(FakeMessagesService::OP_CREATE_PRIV, svc.lastChanOp);
  EXPECT_EQ(1, host->depth());                       // keypad pops regardless of Full (toast shows the error)
}

TEST(MessagesAppletHelpers, IsHexKeyValidation) {
  FakeMessagesService svc; svc.publicJoined = true; svc.chanResult = mishmesh::ChanResult::Ok;
  FakeDisplayDriver d; auto* host = openNewTab(svc, d);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::Select);       // JoinPrivateApplet
  mishmesh::messagesApplet().setChannelNameForTest("p");
  mishmesh::messagesApplet().setChannelKeyForTest("00112233445566778899aabbccddeeg0"); // 'g' invalid
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(0, svc.chanCalls);                          // rejected: non-hex char
}

TEST(MessagesApplet, JoinHashtagOpensKeypadAndJoins) {
  FakeMessagesService svc; svc.publicJoined = true; svc.chanResult = mishmesh::ChanResult::Ok;
  FakeDisplayDriver d; auto* host = openNewTab(svc, d);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);
  host->dispatch(mishmesh::InputEvent::NavDown);    // 4 rows -> row 3 = Join hashtag
  host->dispatch(mishmesh::InputEvent::Select);     // push keypad
  EXPECT_EQ(2, host->depth());
  host->dispatch(mishmesh::InputEvent::Select);     // type a char
  mishmesh::keypadApplet().setFocusForTest(3, 3);   // OK cell
  host->dispatch(mishmesh::InputEvent::Select);     // confirm -> onJoinHashtagDone
  EXPECT_EQ(FakeMessagesService::OP_JOIN_HASH, svc.lastChanOp);
  EXPECT_EQ(1, host->depth());
}

// Regression: a message containing a word longer than wrapText's internal line
// buffer (followed by more text) used to drive a negative-length memcpy and hang
// the device on open. Rendering must complete without crashing.
TEST(MessageThread, LongUnbrokenWordRendersWithoutHang) {
  FakeMessagesService svc;
  auto k = mishmesh::directKey((const uint8_t*)"ALICE!");
  std::string text(120, 'A');     // > 80-byte line buffer
  text += " tail";                // a trailing word triggers the candidate overflow
  svc.store.appendInbound(k, text.c_str(), (uint16_t)text.size(), 1, 1, 0, nullptr, 0);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  mishmesh::messageThreadApplet().setTarget(k);
  host.push(&mishmesh::messageThreadApplet());
  host.loop(0);                   // layoutFocus -> blockHeight -> wrapText
  host.loop(0);                   // a second frame also exercises the draw loop
  SUCCEED();                      // reaching here (no crash/hang) is the assertion
}

// Frees the last openNewTab() host once the suite finishes (ASan hygiene).
class MessagesNewTabEnv : public ::testing::Environment {
 public:
  void TearDown() override { delete gNewTabHost; gNewTabHost = nullptr; }
};

// The outbound status label surfaces the live retry count while a DM is being
// retried, and yields to terminal states (delivered/failed).
TEST(MessageThread, StatusLabelShowsRetryCount) {
  using namespace mishmesh;
  MessageView m{};
  m.outbound = true; m.isChannel = false;
  char buf[28];

  m.status = ST_PENDING; m.retryAttempt = 0;          // just sent, no retry yet
  MessageThreadApplet::statusLabel(m, buf, sizeof(buf));
  EXPECT_STREQ("Sent", buf);

  m.retryAttempt = 2;                                  // mid-retry
  MessageThreadApplet::statusLabel(m, buf, sizeof(buf));
  EXPECT_STREQ("Retrying 2/5", buf);

  m.status = ST_DELIVERED; m.tripTimeMs = 1500; m.retryAttempt = 3;  // delivery wins
  MessageThreadApplet::statusLabel(m, buf, sizeof(buf));
  EXPECT_STREQ("Delivered 1.5s", buf);

  m.status = ST_FAILED; m.retryAttempt = 0;
  MessageThreadApplet::statusLabel(m, buf, sizeof(buf));
  EXPECT_STREQ("Failed", buf);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new MessagesNewTabEnv);
  return RUN_ALL_TESTS();
}
