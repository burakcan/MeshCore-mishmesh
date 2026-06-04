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
  host.dispatch(mishmesh::InputEvent::Select);        // arm the confirm dialog
  EXPECT_TRUE(mishmesh::messagesApplet().menuOpenForTest());   // still open, awaiting confirm
  EXPECT_EQ(2, svc.convoCount());                     // nothing deleted yet
  host.dispatch(mishmesh::InputEvent::NavRight);      // Cancel -> Confirm
  host.dispatch(mishmesh::InputEvent::Select);        // confirm Delete
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
  host.dispatch(mishmesh::InputEvent::Select);     // "Clear chat" (first item) -> confirm dialog
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

// Reply on a channel message seeds "@<sender> " into the compose buffer.
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
  EXPECT_STREQ("@Bob ", mishmesh::keypadApplet().text());
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

// "New group" (row 1) is still a no-op toast (no push).
TEST(MessagesApplet, NewGroupIsNoop) {
  FakeMessagesService svc;
  FakeContactsService contacts;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.messages = &svc; ctx.contacts = &contacts;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::messagesApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);  // -> New tab
  host.dispatch(mishmesh::InputEvent::NavDown);   // -> "New group" (row 1)
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(d0, host.depth());                     // no push
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
