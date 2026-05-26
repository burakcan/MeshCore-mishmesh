#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include "FakeDisplayDriver.h"
#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/applets/ContactDetailApplet.h>
#include <mishmesh/applets/TelemetryApplet.h>
#include <mishmesh/applets/PingApplet.h>
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

  // Move to "Delete contact" (row 4): NavDown x4.
  for (int i = 0; i < 4; i++) host.dispatch(mishmesh::InputEvent::NavDown);
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
  EXPECT_EQ(5, mishmesh::contactDetailApplet().count());
  EXPECT_TRUE(detailHasAction("Clear conversation"));
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
  // View, Telemetry, Ping, Reset path, Delete (no Clear conversation)
  EXPECT_EQ(5, mishmesh::contactDetailApplet().count());
  EXPECT_FALSE(detailHasAction("Clear conversation"));
  EXPECT_TRUE(detailHasAction("Ping"));
  EXPECT_TRUE(detailHasAction("Delete contact"));
}

// ---- Task 12: TelemetryApplet ---------------------------------------------

TEST(TelemetryApplet, RendersFieldsAfterSeqBump) {
  FakeContactsService svc;
  // Pre-load a decoded telemetry view the fake returns.
  svc.telem.valid = true; svc.telem.count = 1;
  svc.telem.fields[0] = mishmesh::TelemetryField{1, 116 /*LPP_VOLTAGE*/, 3.92f, 0, 0};

  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  mishmesh::telemetryApplet().setTarget((const uint8_t*)"ALICE!");  // before onStart fires the request
  host.setRoot(&mishmesh::telemetryApplet());

  // onStart already fired requestTelemetry; simulate response by bumping seq.
  svc.telemSeq = 1;
  host.loop(10);                       // render: should pick up the view
  EXPECT_EQ(1, mishmesh::telemetryApplet().count());
  EXPECT_STREQ("Voltage ch1", mishmesh::telemetryApplet().label(0));
}

TEST(FakeService, RequestTelemetryLogged) {
  FakeContactsService svc;
  svc.requestTelemetry((const uint8_t*)"ALICE!");
  EXPECT_EQ("ALICE!", svc.lastTelemetryReq);
}

TEST(PingApplet, ShowsReplyAfterSeqBump) {
  FakeContactsService svc;
  svc.pingResult.replied = true; svc.pingResult.rttMs = 234;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  mishmesh::pingApplet().setTarget((const uint8_t*)"REP001");  // before onStart fires the ping
  host.setRoot(&mishmesh::pingApplet());
  EXPECT_EQ("REP001", svc.lastPing);   // ping fired on start
  svc.pingSeqVal = 1;                   // simulate reply
  host.loop(10);                        // render picks up the result
  EXPECT_TRUE(d.fills.size() > 0u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
