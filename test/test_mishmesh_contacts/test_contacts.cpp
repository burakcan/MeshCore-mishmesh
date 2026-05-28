#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include "FakeDisplayDriver.h"
#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/widgets/TelemetryDialog.h>
#include <mishmesh/core/AppletHost.h>
#include <cstring>

TEST(ContactsServiceFake, EnumeratesByKind) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6);
  svc.chats.push_back(a);
  EXPECT_EQ(1, svc.countByKind(mishmesh::ContactKind::Chat));
  EXPECT_EQ(0, svc.countByKind(mishmesh::ContactKind::Repeater));
  mishmesh::ContactView v;
  ASSERT_TRUE(svc.getByKind(mishmesh::ContactKind::Chat, 0, v));
  EXPECT_STREQ("Alice", v.name);
}

// ---- Task 10: ContactsApplet ----------------------------------------------

TEST(ContactsApplet, TabSwitchChangesKind) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); svc.chats.push_back(a);
  FakeContactsService::Row r; r.name = "Rep1"; memcpy(r.pubkey, "REP001", 6); svc.repeaters.push_back(r);

  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());

  // Default tab = contacts (Chat). NavRight -> repeaters.
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.loop(1);   // render
  // No crash + still depth 1 (no push on tab change).
  EXPECT_EQ(1, host.depth());
}

TEST(ContactsApplet, SettingsToggleCallsService) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  // Move to settings tab (index 4): NavRight x4.
  for (int i = 0; i < 4; i++) host.dispatch(mishmesh::InputEvent::NavRight);
  // Select first settings row (Users toggle).
  host.dispatch(mishmesh::InputEvent::Select);
  bool called = false;
  for (auto& s : svc.calls) if (s == "setautoadd") called = true;
  EXPECT_TRUE(called);
}

// ---- Task 11: ContactDetailApplet -----------------------------------------

TEST(ContactDetail, DeleteConfirmCallsServiceAndPops) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); svc.chats.push_back(a);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());

  // Open Alice (contacts tab, first row) -> push detail.
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(2, host.depth());

  // Move to "Delete contact" (row 5): View, Favourite, Telemetry, Reset path, Clear, Delete.
  for (int i = 0; i < 5; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);     // opens confirm
  host.dispatch(mishmesh::InputEvent::NavRight);   // select Confirm
  host.dispatch(mishmesh::InputEvent::Select);     // confirm

  EXPECT_EQ("ALICE!", svc.lastDeleted);
  EXPECT_EQ(1, host.depth());   // popped back to list
}

static bool detailHasAction(const char* want) {
  auto& d = mishmesh::contactDetailApplet();
  for (int i = 0; i < d.count(); i++) if (strcmp(d.label(i), want) == 0) return true;
  return false;
}

TEST(ContactDetail, ChatHasClearConversation) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); svc.chats.push_back(a);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.dispatch(mishmesh::InputEvent::Select);   // open Alice (chat)
  EXPECT_EQ(6, mishmesh::contactDetailApplet().count());
  EXPECT_TRUE(detailHasAction("Clear conversation"));
  EXPECT_TRUE(detailHasAction("Add to favorites"));
}

TEST(ContactDetail, RepeaterHasPingNotClearConversation) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "Rep1"; memcpy(r.pubkey, "REP001", 6); svc.repeaters.push_back(r);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);  // -> repeaters tab
  host.dispatch(mishmesh::InputEvent::Select);    // open Rep1
  // View, Favourite, Telemetry, Ping, Reset path, Delete (no Clear conversation)
  EXPECT_EQ(6, mishmesh::contactDetailApplet().count());
  EXPECT_FALSE(detailHasAction("Clear conversation"));
  EXPECT_TRUE(detailHasAction("Ping (0 hop)"));
  EXPECT_TRUE(detailHasAction("Delete contact"));
}

// ---- Task 12: Telemetry modal ---------------------------------------------

TEST(TelemetryDialog, FormatsFieldsGpsOnOwnLine) {
  mishmesh::TelemetryView v; v.valid = true; v.count = 2;
  v.fields[0] = mishmesh::TelemetryField{1, 116 /*LPP_VOLTAGE*/, 3.92f, 0, 0};
  v.fields[1] = mishmesh::TelemetryField{0, 136 /*LPP_GPS*/, 52.1234f, 4.1234f, 0};
  mishmesh::TelemetryDialog dlg;
  dlg.setResult(v);
  // Voltage = 1 line; GPS = 2 lines (label + indented coords) so it isn't ellipsized.
  EXPECT_EQ(3, dlg.lineCount());
}

TEST(ContactDetail, TelemetryOpensModalAndShowsResult) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "Rep1"; memcpy(r.pubkey, "REP001", 6); svc.repeaters.push_back(r);
  svc.telem.valid = true; svc.telem.count = 1;
  svc.telem.fields[0] = mishmesh::TelemetryField{1, 116 /*LPP_VOLTAGE*/, 3.92f, 0, 0};
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);   // -> repeaters tab
  host.dispatch(mishmesh::InputEvent::Select);     // open Rep1 detail
  // Telemetry is action index 2 (View, Favourite, Telemetry, ...): move down twice.
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);     // opens telemetry modal + requests
  EXPECT_EQ("REP001", svc.lastTelemetryReq);
  svc.telemSeq = 1;                                // simulate response
  host.loop(10);                                   // modal picks up the result, stays open (depth 2)
  EXPECT_EQ(2, host.depth());
}

TEST(FakeService, RequestTelemetryLogged) {
  FakeContactsService svc;
  svc.requestTelemetry((const uint8_t*)"ALICE!");
  EXPECT_EQ("ALICE!", svc.lastTelemetryReq);
}

TEST(ContactDetail, PingOpensModalAndFires) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "Rep1"; memcpy(r.pubkey, "REP001", 6); svc.repeaters.push_back(r);
  svc.pingResult.replied = true; svc.pingResult.rttMs = 234; svc.pingResult.snrUs = -7.5f; svc.pingResult.snrThem = -5.0f;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);   // -> repeaters tab
  host.dispatch(mishmesh::InputEvent::Select);     // open Rep1 detail
  // Move to "Ping (0 hop)" (index 3: View, Favourite, Telemetry, Ping) and select it.
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);     // opens the ping modal + fires ping
  EXPECT_EQ("REP001", svc.lastPing);
  svc.pingSeqVal = 1;                              // simulate reply
  host.loop(10);                                   // modal picks up the result, stays open (depth 2)
  EXPECT_EQ(2, host.depth());
}

// ---- Favourites -----------------------------------------------------------

TEST(Favourites, ServiceEnumeratesAcrossKinds) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); a.favourite = true; svc.chats.push_back(a);
  FakeContactsService::Row r; r.name = "Rep1";  memcpy(r.pubkey, "REP001", 6); svc.repeaters.push_back(r);  // not fav
  FakeContactsService::Row s; s.name = "Sense"; memcpy(s.pubkey, "SENS01", 6); s.favourite = true; svc.sensors.push_back(s);
  EXPECT_EQ(2, svc.countFavourites());
  mishmesh::ContactView v;
  EXPECT_TRUE(svc.getFavourite(0, v));  EXPECT_STREQ("Alice", v.name);
  EXPECT_TRUE(svc.getFavourite(1, v));  EXPECT_STREQ("Sense", v.name);
  EXPECT_FALSE(svc.getFavourite(2, v));
}

TEST(ContactDetail, FavouriteToggleAddsToFavouritesTab) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); svc.chats.push_back(a);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  EXPECT_EQ(0, svc.countFavourites());

  host.dispatch(mishmesh::InputEvent::Select);     // open Alice (contacts tab) detail
  host.dispatch(mishmesh::InputEvent::NavDown);     // -> "Add to favorites" (action index 1)
  EXPECT_TRUE(detailHasAction("Add to favorites"));
  host.dispatch(mishmesh::InputEvent::Select);     // toggle favourite ON
  EXPECT_EQ(1, svc.countFavourites());
  EXPECT_TRUE(detailHasAction("Remove from favorites"));

  host.dispatch(mishmesh::InputEvent::Back);        // back to list; onForeground rebuilds tabs
  EXPECT_EQ(1, host.depth());
  // The Favourites tab is now first; navigate to it and open Alice from it.
  host.dispatch(mishmesh::InputEvent::NavLeft);     // Contacts -> Favorites
  host.dispatch(mishmesh::InputEvent::Select);      // open Alice from favourites
  EXPECT_EQ(2, host.depth());
}

TEST(ContactsApplet, OpensOnFavouritesTabWhenFavouritesExist) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); svc.chats.push_back(a);  // not fav, first chat
  FakeContactsService::Row b; b.name = "Bob";   memcpy(b.pubkey, "BOBBBB", 6); b.favourite = true; svc.chats.push_back(b);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  // Active tab is Favourites, whose only row is Bob; opening it lands on Bob, not Alice.
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(2, host.depth());
  EXPECT_TRUE(detailHasAction("Remove from favorites"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
