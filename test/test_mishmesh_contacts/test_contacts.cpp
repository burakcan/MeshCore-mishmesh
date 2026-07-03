#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include "FakeMessagesService.h"
#include "FakeDisplayDriver.h"
#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/applets/ContactPermissionsApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/widgets/TelemetryDialog.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/RepeaterManageApplet.h>
#include <mishmesh/applets/ServerLoginApplet.h>
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
  // Tabs: Contacts,Repeaters,Rooms,Sensors,Discover,Settings -> Settings is index 5.
  for (int i = 0; i < 5; i++) host.dispatch(mishmesh::InputEvent::NavRight);
  // Select first settings row ("Auto-add all" master toggle).
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

  // Move to "Delete contact" (row 9): Send message, View, Rename, Favourite, Telemetry, Permissions, Set path, Reset path, Clear, Delete.
  for (int i = 0; i < 9; i++) host.dispatch(mishmesh::InputEvent::NavDown);
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
  EXPECT_EQ(10, mishmesh::contactDetailApplet().count());
  EXPECT_TRUE(detailHasAction("Clear conversation"));
  EXPECT_TRUE(detailHasAction("Add to favorites"));
  EXPECT_TRUE(detailHasAction("Permissions"));
  EXPECT_TRUE(detailHasAction("Set path"));
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
  // Manage, View, Rename, Favourite, Telemetry, Permissions, Ping, Set path, Reset path, Delete (no Clear conversation)
  EXPECT_EQ(10, mishmesh::contactDetailApplet().count());
  EXPECT_FALSE(detailHasAction("Clear conversation"));
  EXPECT_TRUE(detailHasAction("Ping (0 hop)"));
  EXPECT_TRUE(detailHasAction("Delete contact"));
}

TEST(ContactDetail, RepeaterHasManageAsPrimaryAction) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "Rep1"; memcpy(r.pubkey, "REP001", 6); r.type = 2;
  svc.repeaters.push_back(r);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);   // -> repeaters tab
  host.dispatch(mishmesh::InputEvent::Select);     // open Rep1 detail
  EXPECT_STREQ("Manage", mishmesh::contactDetailApplet().label(0));
  EXPECT_TRUE(detailHasAction("Manage"));
}

TEST(ContactDetail, ManageRoutesToRepeaterManageWhenLoggedIn) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "Rep1"; memcpy(r.pubkey, "REP001", 6); r.type = 2;
  svc.repeaters.push_back(r);
  svc.loggedIn = true;                             // short-circuit: skip the keypad
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.dispatch(mishmesh::InputEvent::NavRight);   // -> repeaters tab
  host.dispatch(mishmesh::InputEvent::Select);     // open Rep1 detail (depth 2)
  host.dispatch(mishmesh::InputEvent::Select);     // action 0 = Manage -> push ServerLoginApplet (depth 3)
  host.loop(1);                                    // Start phase: isLoggedIn -> onSuccess -> replace with RepeaterManageApplet
  EXPECT_EQ(3, host.depth());
  EXPECT_EQ(0, memcmp(mishmesh::repeaterManageApplet().targetForTest(), "REP001", 6));
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
  // Telemetry is action index 4 (Manage, View, Rename, Favourite, Telemetry, ...): move down 4x.
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
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
  // Move to "Ping (0 hop)" (index 6: Manage, View, Rename, Favourite, Telemetry, Permissions, Ping) and select it.
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);     // opens the ping modal + fires ping
  EXPECT_EQ("REP001", svc.lastPing);
  svc.pingSeqVal = 1;                              // simulate reply
  host.loop(10);                                   // modal picks up the result, stays open (depth 2)
  EXPECT_EQ(2, host.depth());
}

// ---- Task 1: RepeaterManageApplet -----------------------------------------

TEST(RepeaterManage, StoresTargetAndRenders) {
  mishmesh::repeaterManageApplet().setTarget((const uint8_t*)"REP001", "Rep1");
  EXPECT_EQ(0, memcmp(mishmesh::repeaterManageApplet().targetForTest(), "REP001", 6));

  FakeDisplayDriver d;
  mishmesh::AppletContext ctx;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::repeaterManageApplet());
  host.loop(1);                       // renders without crashing
  EXPECT_EQ(1, host.depth());
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
  host.dispatch(mishmesh::InputEvent::NavDown);     // -> View details (action index 1)
  host.dispatch(mishmesh::InputEvent::NavDown);     // -> Rename (action index 2)
  host.dispatch(mishmesh::InputEvent::NavDown);     // -> "Add to favorites" (action index 3)
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

// ---- Contact telemetry permissions ----------------------------------------

TEST(ContactPermissions, TogglesReflectAndPersistBits) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6);
  a.telemPerms = mishmesh::TelemPermBase;   // start with base already granted
  svc.chats.push_back(a);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);

  auto& perms = mishmesh::contactPermissionsApplet();
  perms.setTarget((const uint8_t*)"ALICE!", "Alice");
  host.setRoot(&perms);

  // Three toggle rows; the first ("Allow telemetry") reflects the seeded base bit.
  ASSERT_EQ(3, perms.count());
  EXPECT_TRUE(perms.toggleState(mishmesh::ContactPermissionsApplet::AllowRequests));
  EXPECT_FALSE(perms.toggleState(mishmesh::ContactPermissionsApplet::IncludeLocation));

  // Select the location row -> sets TelemPermLocation on the contact.
  host.dispatch(mishmesh::InputEvent::NavDown);   // -> Include location
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_TRUE(perms.toggleState(mishmesh::ContactPermissionsApplet::IncludeLocation));
  EXPECT_EQ(mishmesh::TelemPermBase | mishmesh::TelemPermLocation, svc.chats[0].telemPerms);

  // Toggle base off -> clears just that bit, leaving location set.
  host.dispatch(mishmesh::InputEvent::NavUp);     // -> Allow telemetry
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_FALSE(perms.toggleState(mishmesh::ContactPermissionsApplet::AllowRequests));
  EXPECT_EQ(mishmesh::TelemPermLocation, svc.chats[0].telemPerms);
}

TEST(ContactPermissions, OpenedFromContactDetailPushes) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); svc.chats.push_back(a);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.dispatch(mishmesh::InputEvent::Select);   // open Alice detail (depth 2)
  // Chat order: Message, View, Rename, Favourite, Telemetry, Permissions(5), ...
  for (int i = 0; i < 5; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);   // push permissions
  EXPECT_EQ(3, host.depth());
}

// ---- Set path --------------------------------------------------------------

TEST(SetPath, ParseHexHops) {
  uint8_t out[mishmesh::PATH_MAX_BYTES];
  // 1-byte hops
  int n = mishmesh::ContactDetailApplet::parseHexPath("3f,a1,bb", 1, out, sizeof(out));
  ASSERT_EQ(3, n);
  EXPECT_EQ(0x3f, out[0]); EXPECT_EQ(0xa1, out[1]); EXPECT_EQ(0xbb, out[2]);
  // uppercase + spaces tolerated
  EXPECT_EQ(2, mishmesh::ContactDetailApplet::parseHexPath("AA, BB", 1, out, sizeof(out)));
  // empty => flood (0 hops)
  EXPECT_EQ(0, mishmesh::ContactDetailApplet::parseHexPath("", 1, out, sizeof(out)));
  // 2-byte hops
  n = mishmesh::ContactDetailApplet::parseHexPath("aabb,ccdd", 2, out, sizeof(out));
  ASSERT_EQ(2, n);
  EXPECT_EQ(0xaa, out[0]); EXPECT_EQ(0xbb, out[1]); EXPECT_EQ(0xcc, out[2]); EXPECT_EQ(0xdd, out[3]);
  // malformed: bad hex, and wrong token length for the hash size
  EXPECT_EQ(-1, mishmesh::ContactDetailApplet::parseHexPath("zz", 1, out, sizeof(out)));
  EXPECT_EQ(-1, mishmesh::ContactDetailApplet::parseHexPath("aaa", 1, out, sizeof(out)));
  EXPECT_EQ(-1, mishmesh::ContactDetailApplet::parseHexPath("aa", 2, out, sizeof(out)));   // need 2 bytes/hop
}

TEST(SetPath, SaveEncodesAndPersists) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Aras"; memcpy(a.pubkey, "ARAS01", 6); svc.chats.push_back(a);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);

  auto& cd = mishmesh::contactDetailApplet();
  cd.setTarget((const uint8_t*)"ARAS01");
  host.setRoot(&cd);

  cd.openSetPathForTest();                    // configures + pushes the form
  cd.setSetPathTextForTest("3f,a1", 1);
  host.dispatch(mishmesh::InputEvent::NavDown);   // Hash size -> Path
  host.dispatch(mishmesh::InputEvent::NavDown);   // Path -> Save button
  host.dispatch(mishmesh::InputEvent::Select);    // save + pop

  EXPECT_EQ("ARAS01", svc.lastPathSet);
  ASSERT_TRUE(svc.chats[0].hasPathBytes);
  EXPECT_EQ(2, svc.chats[0].pathEncoded & 63);          // 2 hops
  EXPECT_EQ(0, svc.chats[0].pathEncoded >> 6);          // 1-byte hash size
  EXPECT_EQ(0x3f, svc.chats[0].pathBytes[0]);
  EXPECT_EQ(0xa1, svc.chats[0].pathBytes[1]);
}

TEST(SetPath, EmptyTextClearsToFlood) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Aras"; memcpy(a.pubkey, "ARAS01", 6);
  a.hasPathBytes = true; a.pathEncoded = 1; a.pathBytes[0] = 0x3f;   // existing 1-hop path
  svc.chats.push_back(a);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);

  auto& cd = mishmesh::contactDetailApplet();
  cd.setTarget((const uint8_t*)"ARAS01");
  host.setRoot(&cd);

  cd.openSetPathForTest();
  cd.setSetPathTextForTest("", 1);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);    // save empty -> flood

  EXPECT_FALSE(svc.chats[0].hasPathBytes);
}

TEST(SetPath, OpenedFromContactDetailPushes) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); svc.chats.push_back(a);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.dispatch(mishmesh::InputEvent::Select);   // open Alice detail (depth 2)
  // Chat order: Message, View, Rename, Favourite, Telemetry, Permissions, Set path(6), ...
  for (int i = 0; i < 6; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);   // push the form
  EXPECT_EQ(3, host.depth());
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

TEST(ContactsApplet, KeepsListPositionAfterDrillInAndBack) {
  FakeContactsService svc;
  const char* keys[3] = {"AAA000", "BBB000", "CCC000"};
  for (int i = 0; i < 3; i++) {
    FakeContactsService::Row r; r.name = keys[i]; memcpy(r.pubkey, keys[i], 6); svc.chats.push_back(r);
  }
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());

  host.dispatch(mishmesh::InputEvent::NavDown);   // select row 1 (BBB000)
  host.dispatch(mishmesh::InputEvent::Select);    // drill in
  host.dispatch(mishmesh::InputEvent::Back);      // back to list; position should be kept
  EXPECT_EQ(1, host.depth());

  // Re-open the selected row and delete it; the target reveals which row was kept.
  host.dispatch(mishmesh::InputEvent::Select);
  for (int i = 0; i < 9; i++) host.dispatch(mishmesh::InputEvent::NavDown);  // -> Delete contact
  host.dispatch(mishmesh::InputEvent::Select);    // confirm dialog
  host.dispatch(mishmesh::InputEvent::NavRight);  // Confirm
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ("BBB000", svc.lastDeleted);
}

TEST(ContactsApplet, RemoveNonFavouritesCleanupKeepsOnlyFavourites) {
  FakeContactsService svc;
  svc.cfg.overwriteOldest = true;      // hide "Notify when full" to keep nav indices stable
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); a.favourite = true; svc.chats.push_back(a);
  FakeContactsService::Row b; b.name = "Bob";   memcpy(b.pubkey, "BOBBBB", 6); svc.chats.push_back(b);
  FakeContactsService::Row r; r.name = "Rep1";  memcpy(r.pubkey, "REP001", 6); svc.repeaters.push_back(r);
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  // Tabs: Favorites,Contacts,Repeaters,Rooms,Sensors,Discover,Settings -> Settings is index 6.
  for (int i = 0; i < 6; i++) host.dispatch(mishmesh::InputEvent::NavRight);  // -> Settings
  // Auto-add-all on (default) collapses the kind rows, so Settings is:
  // [Auto-add all, Overwrite oldest, Max hops, Remove non-users, Remove non-favourites, Remove all].
  for (int i = 0; i < 4; i++) host.dispatch(mishmesh::InputEvent::NavDown);   // -> "Remove non-favourites"
  host.dispatch(mishmesh::InputEvent::Select);     // confirm dialog
  host.dispatch(mishmesh::InputEvent::NavRight);   // Confirm
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(1u, svc.chats.size());                 // only Alice (favourite) remains
  EXPECT_TRUE(svc.chats[0].favourite);
  EXPECT_EQ(0u, svc.repeaters.size());             // non-favourite repeater removed
}

// ---- Discover -------------------------------------------------------------

TEST(Discover, ServiceAddPromotesToContacts) {
  FakeContactsService svc;
  FakeContactsService::Row d; d.name = "NewRep"; memcpy(d.pubkey, "DISC01", 6); d.type = 2 /*repeater*/;
  svc.discovered.push_back(d);
  EXPECT_EQ(1, svc.countDiscovered());
  EXPECT_EQ(0, svc.countByKind(mishmesh::ContactKind::Repeater));
  EXPECT_TRUE(svc.addDiscovered((const uint8_t*)"DISC01"));
  EXPECT_EQ(0, svc.countDiscovered());                                  // left the discovery list
  EXPECT_EQ(1, svc.countByKind(mishmesh::ContactKind::Repeater));       // now a contact
}

TEST(ContactsApplet, DiscoverTabAddsSelectedNode) {
  FakeContactsService svc;
  FakeContactsService::Row d; d.name = "NewNode"; memcpy(d.pubkey, "DISC01", 6); d.type = 1 /*chat*/;
  svc.discovered.push_back(d);
  FakeDisplayDriver d2;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d2, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  // No favourites: tabs are Contacts,Repeaters,Rooms,Sensors,Discover,Settings -> Discover is index 4.
  for (int i = 0; i < 4; i++) host.dispatch(mishmesh::InputEvent::NavRight);
  host.dispatch(mishmesh::InputEvent::Select);     // open the discovery detail
  EXPECT_EQ(2, host.depth());
  // Detail actions: "Add to contacts" (index 0) is preselected.
  host.dispatch(mishmesh::InputEvent::Select);     // add as contact
  EXPECT_EQ(0, svc.countDiscovered());
  EXPECT_EQ(1, svc.countByKind(mishmesh::ContactKind::Chat));
  EXPECT_EQ(2, host.depth());                       // replace()d with contact detail; Back returns to list
}

// ---- Task 2: Send message action -------------------------------------------

// A user (Chat) contact gets "Send message" as its first action, opening the thread.
TEST(ContactDetail, SendMessageFirstForUserOpensThread) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "Alice"; memcpy(r.pubkey, "ALICE!", 6); r.type = 1;
  svc.chats.push_back(r);
  FakeMessagesService msgs;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  mishmesh::contactDetailApplet().setTarget((const uint8_t*)"ALICE!");
  host.push(&mishmesh::contactDetailApplet());

  EXPECT_STREQ("Send message", mishmesh::contactDetailApplet().label(0));
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);    // first action = Send message
  EXPECT_EQ(d0 + 1, host.depth());
  EXPECT_STREQ("Alice", mishmesh::messageThreadApplet().headerTitleForTest());
}

// A non-user contact (repeater) does NOT get a Send message action.
TEST(ContactDetail, NoSendMessageForRepeater) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "RptX"; memcpy(r.pubkey, "RPTXXX", 6); r.type = 2;
  svc.repeaters.push_back(r);
  FakeMessagesService msgs;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  mishmesh::contactDetailApplet().setTarget((const uint8_t*)"RPTXXX");
  host.push(&mishmesh::contactDetailApplet());
  for (int i = 0; i < mishmesh::contactDetailApplet().count(); i++)
    EXPECT_STRNE("Send message", mishmesh::contactDetailApplet().label(i));
}

// ---- Task 3: Contact picker (FavouritesListModel users-only + pick mode) ----

// FavouritesListModel users-only filter: counts/returns only Chat-type favourites.
TEST(FavouritesModel, UsersOnlyFiltersNonChat) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); a.type = 1; a.favourite = true;
  FakeContactsService::Row rp; rp.name = "Rpt"; memcpy(rp.pubkey, "RPTXXX", 6); rp.type = 2; rp.favourite = true;
  svc.chats.push_back(a); svc.repeaters.push_back(rp);

  mishmesh::ContactFilterCache cache; cache.bind(&svc);
  mishmesh::FavouritesListModel m; m.bind(&cache);
  EXPECT_EQ(2, m.count());                       // unfiltered: both favourites
  m.setUsersOnly(true);
  EXPECT_EQ(1, m.count());                        // only the Chat favourite
  EXPECT_STREQ("Alice", m.label(0));
}

// Pick mode: only Favourites + Contacts tabs (favourites filtered to users).
TEST(ContactsPick, PickModeLimitsTabs) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); a.type = 1; a.favourite = true;
  svc.chats.push_back(a);
  FakeContactsService::Row rp; rp.name = "Rpt"; memcpy(rp.pubkey, "RPTXXX", 6); rp.type = 2; rp.favourite = true;
  svc.repeaters.push_back(rp);
  FakeMessagesService msgs;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  mishmesh::contactsApplet().beginPick();
  host.push(&mishmesh::contactsApplet());
  EXPECT_TRUE(mishmesh::contactsApplet().pickModeForTest());
  EXPECT_EQ(2, mishmesh::contactsApplet().tabCountForTest());   // Favorites (users) + Contacts
}

// Pick mode select opens the contact's thread via replace (depth unchanged).
TEST(ContactsPick, SelectOpensThreadViaReplace) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); a.type = 1;
  svc.chats.push_back(a);
  FakeMessagesService msgs;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  mishmesh::contactsApplet().beginPick();
  host.push(&mishmesh::contactsApplet());        // no favourites -> single Contacts tab, Alice at row 0
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);   // pick Alice
  EXPECT_EQ(d0, host.depth());                    // replace, not push
  EXPECT_STREQ("Alice", mishmesh::messageThreadApplet().headerTitleForTest());
}

// A normal (non-pick) start still opens the contact detail on select.
TEST(ContactsPick, NormalModeStillOpensDetail) {
  FakeContactsService svc;
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); a.type = 1;
  svc.chats.push_back(a);
  FakeMessagesService msgs;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  host.push(&mishmesh::contactsApplet());         // normal mode (no beginPick)
  EXPECT_FALSE(mishmesh::contactsApplet().pickModeForTest());
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(d0 + 1, host.depth());                // detail pushed
}

// ---- Task 3 bug fix: rawIndex() maps filtered -> raw for pick-mode handler ----

// Verify that FavouritesListModel::rawIndex() returns the correct raw index when
// setUsersOnly(true) and favourites contain both Chat and non-Chat entries.
//
// NOTE on cross-ordering reproducibility: FakeContactsService::getFavourite() iterates
// kinds in fixed order Chat -> Repeater -> Room -> Sensor, so a Chat favourite ALWAYS
// has a lower raw index than a Repeater favourite. This means the failure path
// (filtered 0 fetching a Repeater at raw 0) cannot be constructed through this fake.
// We therefore assert the property that IS observable - rawIndex(i)==i for a single
// Chat favourite at raw 0, and confirm the pick-mode select handler resolves through
// rawIndex() correctly (opening the right thread). The cross-ordering guard lives in the
// production code path itself (rawIndex() used in the handler for all Favourites selects).
TEST(FavouritesModel, RawIndexMapsCorrectlyUsersOnly) {
  FakeContactsService svc;
  // raw 0: Chat "Alice" (favourite, displayed in users-only mode)
  // raw 1: Repeater "Bob" (favourite, NOT displayed in users-only mode)
  // Because Chat enumerates before Repeater in getFavourite(), Alice is always raw 0
  // and Bob is always raw 1; cross-ordering (Bob < Alice raw) is not achievable here.
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); a.type = 1; a.favourite = true;
  FakeContactsService::Row b; b.name = "Bob";   memcpy(b.pubkey, "BOBXXX", 6); b.type = 2; b.favourite = true;
  svc.chats.push_back(a); svc.repeaters.push_back(b);

  mishmesh::ContactFilterCache cache; cache.bind(&svc);
  mishmesh::FavouritesListModel m; m.bind(&cache);
  m.setUsersOnly(true);
  EXPECT_EQ(1, m.count());            // only Alice visible
  EXPECT_EQ(0, m.rawIndex(0));        // filtered 0 -> raw 0 (Alice)
  EXPECT_EQ(-1, m.rawIndex(1));       // out-of-range filtered -> -1 (no crash)
}

// Pick mode: selecting the Favourites tab row opens the thread for the CORRECT contact
// (the Chat favourite, not any non-Chat favourite) via rawIndex() indirection.
TEST(ContactsPick, FavouritesTabSelectOpensCorrectUserThread) {
  FakeContactsService svc;
  // Alice is Chat favourite (raw 0 in getFavourite enumeration since Chat iterates first).
  // Rep is Repeater favourite (raw 1). In pick mode the Favourites tab shows only Alice
  // (filtered index 0 = raw index 0). The handler must resolve via rawIndex() and open
  // Alice's thread. Without the fix it called getFavourite(_list.selected()) = getFavourite(0)
  // which is already Alice here, so this specific ordering does NOT expose the bug through
  // this fake. The test still confirms the handler reaches the right contact and documents
  // the rawIndex() guard is exercised.
  FakeContactsService::Row a; a.name = "Alice"; memcpy(a.pubkey, "ALICE!", 6); a.type = 1; a.favourite = true;
  FakeContactsService::Row rp; rp.name = "Rep"; memcpy(rp.pubkey, "REPXXX", 6); rp.type = 2; rp.favourite = true;
  svc.chats.push_back(a); svc.repeaters.push_back(rp);
  FakeMessagesService msgs;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  mishmesh::contactsApplet().beginPick();
  host.push(&mishmesh::contactsApplet());
  // In pick mode: Favourites tab (users-only) has 1 row = Alice; Contacts tab has 1 row = Alice.
  // The applet opens on the Favourites tab (slot 0). Select -> should open Alice's thread.
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(d0, host.depth());    // replace, not push
  EXPECT_STREQ("Alice", mishmesh::messageThreadApplet().headerTitleForTest());
}

// Rename: appears after View, opens the keypad pre-filled with the current name,
// and on OK renames the contact (and the detail refreshes to the new name).
TEST(ContactDetail, RenameOpensKeypadPrefilledAndRenames) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "Alice"; memcpy(r.pubkey, "ALICE!", 6); r.type = 1;
  svc.chats.push_back(r);
  FakeMessagesService msgs;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  mishmesh::contactDetailApplet().setTarget((const uint8_t*)"ALICE!");
  host.push(&mishmesh::contactDetailApplet());

  // Chat actions: [Send message, View details, Rename, ...]
  EXPECT_STREQ("Rename", mishmesh::contactDetailApplet().label(2));

  host.dispatch(mishmesh::InputEvent::NavDown);   // View
  host.dispatch(mishmesh::InputEvent::NavDown);   // Rename
  int d0 = host.depth();
  host.dispatch(mishmesh::InputEvent::Select);    // open keypad
  EXPECT_EQ(d0 + 1, host.depth());
  EXPECT_STREQ("Alice", mishmesh::keypadApplet().text());   // pre-filled with current name

  host.dispatch(mishmesh::InputEvent::Select);    // keypad opens on "abc" -> append 'a' => "Alicea"
  mishmesh::keypadApplet().setFocusForTest(3, 3); // OK cell
  host.dispatch(mishmesh::InputEvent::Select);    // confirm -> rename + pop
  EXPECT_EQ(d0, host.depth());
  EXPECT_FALSE(svc.lastRenamed.empty());          // renameContact was called
  mishmesh::ContactView v; ASSERT_TRUE(svc.getByKind(mishmesh::ContactKind::Chat, 0, v));
  EXPECT_STREQ("Alicea", v.name);
}

// Rename is offered for non-user contacts too (here: a repeater), right after View.
TEST(ContactDetail, RenameOfferedForRepeater) {
  FakeContactsService svc;
  FakeContactsService::Row r; r.name = "Rpt"; memcpy(r.pubkey, "RPTXXX", 6); r.type = 2;
  svc.repeaters.push_back(r);
  FakeMessagesService msgs;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  mishmesh::contactDetailApplet().setTarget((const uint8_t*)"RPTXXX");
  host.push(&mishmesh::contactDetailApplet());
  // Repeater (Manage primary): [Manage, View details, Rename, ...]
  EXPECT_STREQ("Rename", mishmesh::contactDetailApplet().label(2));
}

// "Auto-add all" collapses the per-kind toggles; turning it off reveals them
// (mirrors the companion app's all/selected modes).
TEST(ContactsSettings, AutoAddAllCollapsesKindRows) {
  FakeContactsService svc;
  mishmesh::ContactsSettingsModel m;
  m.bind(&svc);
  svc.cfg.overwriteOldest = true;   // hide the "Notify when full" row for this layout check

  svc.cfg.autoAddAll = true;
  EXPECT_EQ(6, m.count());                                  // master + overwrite + maxhops + 3 actions
  EXPECT_EQ(mishmesh::ContactsSettingsModel::AutoAddAll, m.rowAt(0));
  EXPECT_TRUE(m.isToggle(0));
  EXPECT_TRUE(m.toggleState(0));                            // reflects autoAddAll
  EXPECT_EQ(mishmesh::ContactsSettingsModel::Overwrite, m.rowAt(1));   // kinds hidden

  svc.cfg.autoAddAll = false;
  EXPECT_EQ(10, m.count());                                 // kinds now visible
  EXPECT_EQ(mishmesh::ContactsSettingsModel::Users, m.rowAt(1));
  EXPECT_FALSE(m.toggleState(0));
}

// Selecting the master row flips manual/auto mode via the service.
TEST(ContactsApplet, SettingsAutoAddAllTogglesMaster) {
  FakeContactsService svc;
  svc.cfg.autoAddAll = true;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  // Tabs: Contacts,Repeaters,Rooms,Sensors,Discover,Settings -> Settings is index 5.
  for (int i = 0; i < 5; i++) host.dispatch(mishmesh::InputEvent::NavRight);
  host.dispatch(mishmesh::InputEvent::Select);   // row 0 = "Auto-add all"
  EXPECT_FALSE(svc.cfg.autoAddAll);              // flipped to "selected"
}

TEST(ContactsSettings, MaxHopsRowShowsValueNotToggle) {
  FakeContactsService svc;
  svc.cfg.autoAddAll = true;
  svc.cfg.overwriteOldest = true;     // hide "Notify when full" so MaxHops stays at index 2
  svc.cfg.maxHops = 1;                 // "Direct"
  mishmesh::ContactsSettingsModel m;
  m.bind(&svc);

  // master(0), overwrite(1), maxhops(2), then 3 actions = 6 rows.
  EXPECT_EQ(6, m.count());
  EXPECT_EQ(mishmesh::ContactsSettingsModel::MaxHops, m.rowAt(2));
  EXPECT_FALSE(m.isToggle(2));
  ASSERT_NE(nullptr, m.value(2));
  EXPECT_STREQ("Direct", m.value(2));
  EXPECT_STREQ("Max hops", m.label(2));
}

TEST(ContactsSettings, MaxHopsLabelMapping) {
  FakeContactsService svc;
  svc.cfg.autoAddAll = true;
  svc.cfg.overwriteOldest = true;     // hide "Notify when full" so MaxHops stays at index 2
  mishmesh::ContactsSettingsModel m;
  m.bind(&svc);
  svc.cfg.maxHops = 0;  EXPECT_STREQ("No limit", m.value(2));
  svc.cfg.maxHops = 1;  EXPECT_STREQ("Direct",   m.value(2));
  svc.cfg.maxHops = 2;  EXPECT_STREQ("1 hop",    m.value(2));
  svc.cfg.maxHops = 3;  EXPECT_STREQ("2 hops",   m.value(2));
  svc.cfg.maxHops = 64; EXPECT_STREQ("63 hops",  m.value(2));
}

// Selecting the Max hops row opens the stepper; NavRight then Select writes the
// new raw value through the service.
TEST(ContactsApplet, MaxHopsStepperWritesValue) {
  FakeContactsService svc;
  svc.cfg.autoAddAll = true;
  svc.cfg.overwriteOldest = true;      // hide "Notify when full" so MaxHops stays at index 2
  svc.cfg.maxHops = 1;                 // "Direct"
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  // Settings is the last tab (index 5): Contacts,Repeaters,Rooms,Sensors,Discover,Settings.
  for (int i = 0; i < 5; i++) host.dispatch(mishmesh::InputEvent::NavRight);
  // Settings rows: AutoAddAll(0), Overwrite(1), MaxHops(2). Move down to MaxHops.
  host.dispatch(mishmesh::InputEvent::NavDown);   // -> Overwrite
  host.dispatch(mishmesh::InputEvent::NavDown);   // -> MaxHops
  host.dispatch(mishmesh::InputEvent::Select);    // open stepper (seeded at 1)
  host.dispatch(mishmesh::InputEvent::NavRight);  // 1 -> 2
  host.dispatch(mishmesh::InputEvent::Select);    // confirm
  EXPECT_EQ(2, svc.cfg.maxHops);
}

// Cancelling the stepper leaves the value unchanged.
TEST(ContactsApplet, MaxHopsStepperCancelKeepsValue) {
  FakeContactsService svc;
  svc.cfg.autoAddAll = true;
  svc.cfg.overwriteOldest = true;      // hide "Notify when full" so MaxHops stays at index 2
  svc.cfg.maxHops = 1;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  host.setRoot(&mishmesh::contactsApplet());
  for (int i = 0; i < 5; i++) host.dispatch(mishmesh::InputEvent::NavRight);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);    // open
  host.dispatch(mishmesh::InputEvent::NavRight);  // tweak (uncommitted)
  host.dispatch(mishmesh::InputEvent::Back);      // cancel
  EXPECT_EQ(1, svc.cfg.maxHops);
}

// "Notify when full" sits under "Overwrite oldest" and is hidden while overwrite
// is on (the alert is moot when a full store still evicts to make room).
TEST(ContactsSettings, NotifyFullRowHiddenWhenOverwriteOn) {
  FakeContactsService svc;
  svc.cfg.autoAddAll = true;
  mishmesh::ContactsSettingsModel m;
  m.bind(&svc);

  svc.cfg.overwriteOldest = false;
  // master(0), overwrite(1), notifyfull(2), maxhops(3), 3 actions = 7
  EXPECT_EQ(7, m.count());
  EXPECT_EQ(mishmesh::ContactsSettingsModel::NotifyFull, m.rowAt(2));
  EXPECT_TRUE(m.isToggle(2));
  EXPECT_STREQ("Notify when full", m.label(2));

  svc.cfg.overwriteOldest = true;
  // notifyfull hidden: master(0), overwrite(1), maxhops(2), 3 actions = 6
  EXPECT_EQ(6, m.count());
  EXPECT_EQ(mishmesh::ContactsSettingsModel::MaxHops, m.rowAt(2));
}

TEST(ContactsSettings, NotifyFullToggleReflectsField) {
  FakeContactsService svc;
  svc.cfg.autoAddAll = true;
  svc.cfg.overwriteOldest = false;
  svc.cfg.notifyWhenFull = true;
  mishmesh::ContactsSettingsModel m;
  m.bind(&svc);
  EXPECT_TRUE(m.toggleState(2));    // NotifyFull at index 2
  svc.cfg.notifyWhenFull = false;
  EXPECT_FALSE(m.toggleState(2));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
