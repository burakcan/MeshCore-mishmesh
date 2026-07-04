#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include <cstring>
#include "FakeDisplayDriver.h"
#include "FakeMessagesService.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/ContactsApplet.h>
#include <mishmesh/applets/RepeaterAclApplet.h>

static uint8_t g_picked[6]; static bool g_pickFired = false;
static void onPickCb(void* ctx, const uint8_t* pk) { (void)ctx; memcpy(g_picked, pk, 6); g_pickFired = true; }

TEST(ContactsPick, CallbackFiresWithPubkey) {
  FakeContactsService svc; FakeMessagesService msgs; FakeDisplayDriver d;
  FakeContactsService::Row r; r.name = "Alice"; memcpy(r.pubkey, "ALICE!", 6); svc.chats.push_back(r);
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  g_pickFired = false;
  mishmesh::contactsApplet().configurePick(&onPickCb, nullptr);
  host.setRoot(&mishmesh::contactsApplet());
  host.loop(1);
  // Default tab shows chats; Select the first contact -> callback fires + pops.
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);
  EXPECT_TRUE(g_pickFired);
  EXPECT_EQ(0, memcmp(g_picked, "ALICE!", 6));
}

TEST(FakeAcl, RequestAndLatest) {
  FakeContactsService svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  EXPECT_TRUE(svc.requestAccessList(pk));
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastAclReq);
  uint32_t before = svc.accessListSeq();

  mishmesh::AccessListView v{}; v.valid = true; v.count = 1;
  memcpy(v.entries[0].pubkey, "AABBCC", 6); v.entries[0].perms = 2;
  svc.simulateAccessList(pk, v);
  EXPECT_NE(before, svc.accessListSeq());

  mishmesh::AccessListView out{};
  ASSERT_TRUE(svc.latestAccessList(pk, out));
  EXPECT_EQ(1, out.count);
  EXPECT_EQ(2, out.entries[0].perms);

  uint8_t other[6] = {'X','X','X','X','X','X'};
  EXPECT_FALSE(svc.latestAccessList(other, out));
}

TEST(ContactsPick, BeginPickClearsStaleCallback) {
  FakeContactsService svc; FakeMessagesService msgs; FakeDisplayDriver d;
  FakeContactsService::Row r; r.name = "Bob"; memcpy(r.pubkey, "BOBBB!", 6); svc.chats.push_back(r);
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  g_pickFired = false;
  // 1) configurePick then cancel (Back) without selecting
  mishmesh::contactsApplet().configurePick(&onPickCb, nullptr);
  host.setRoot(&mishmesh::contactsApplet());
  host.loop(1);
  host.dispatch(mishmesh::InputEvent::Back);   // cancel; may pop or stay at root
  host.loop(2);
  // 2) now a normal beginPick() session (new-message picker)
  mishmesh::contactsApplet().beginPick();
  host.setRoot(&mishmesh::contactsApplet());   // re-open (root reset ok in host test)
  host.loop(3);
  host.dispatch(mishmesh::InputEvent::Select); // select Bob -> default thread path, NOT the ACL callback
  host.loop(4);
  EXPECT_FALSE(g_pickFired);                    // stale callback did NOT fire
}

static void fillAcl(FakeContactsService& svc, const uint8_t* pk) {
  mishmesh::AccessListView v{}; v.valid = true; v.count = 1;
  memcpy(v.entries[0].pubkey, "\xAA\xBB\xCC\xDD\xEE\xFF", 6); v.entries[0].perms = 2;  // read-write
  svc.simulateAccessList(pk, v);
}

TEST(RepeaterAcl, FetchRendersEntries) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterAclApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterAclApplet());
  host.loop(1);
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastAclReq);
  fillAcl(svc, pk);
  host.loop(300);
  EXPECT_EQ(1, mishmesh::repeaterAclApplet().entryCountForTest());
  EXPECT_EQ(2, mishmesh::repeaterAclApplet().entryPermsForTest(0));
}

TEST(RepeaterAcl, SelectEntryThenLevelFiresSetperm) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterAclApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterAclApplet());
  host.loop(1); fillAcl(svc, pk); host.loop(300);
  // Row 0 = the entry -> opens the level chooser (Read-only/Read-write/Admin/Remove).
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);
  // Chooser row 0 = Read-only (perms 1) -> setperm <hex> 1.
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(0, strncmp(svc.lastCli.c_str(), "setperm ", 8));
  EXPECT_NE(std::string::npos, svc.lastCli.find(" 1"));    // read-only level
}

// "Add user" -> pick a contact -> ACL applet reaches Level phase with _target set.
// This tests the callback-before-pop ordering fix in ContactsApplet.
TEST(RepeaterAcl, AddUserPickReachesLevelPhase) {
  FakeContactsService svc; FakeMessagesService msgs; FakeDisplayDriver d;
  FakeContactsService::Row cr; cr.name = "Charlie"; memcpy(cr.pubkey, "CHRL01", 6); svc.chats.push_back(cr);
  mishmesh::AppletContext ctx; ctx.contacts = &svc; ctx.messages = &msgs;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','2'};
  mishmesh::repeaterAclApplet().setTarget(pk, "Rep2");
  host.setRoot(&mishmesh::repeaterAclApplet());
  host.loop(1);
  // While fetching, phase is Loading (loading screen visible, not the list).
  EXPECT_EQ(mishmesh::RepeaterAclApplet::Phase::Loading, mishmesh::repeaterAclApplet().phaseForTest());
  fillAcl(svc, pk);
  host.loop(300);
  EXPECT_EQ(mishmesh::RepeaterAclApplet::Phase::List, mishmesh::repeaterAclApplet().phaseForTest());
  EXPECT_EQ(1, mishmesh::repeaterAclApplet().entryCountForTest());
  // Navigate to "Add user" (row 1, after the one ACL entry).
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);     // pushes contactsApplet in pick mode
  EXPECT_EQ(2, host.depth());
  // Pick Charlie (first row in Contacts tab); callback fires, ACL's onForeground sees _pickReady=true.
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(1, host.depth());
  EXPECT_EQ(mishmesh::RepeaterAclApplet::Phase::Level, mishmesh::repeaterAclApplet().phaseForTest());
}

int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
