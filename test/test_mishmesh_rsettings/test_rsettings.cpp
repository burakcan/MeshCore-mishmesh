#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include <mishmesh/core/RemoteSettings.h>
#include <cstring>
#include "FakeDisplayDriver.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/RepeaterSettingsPanel.h>
#include <mishmesh/applets/RepeaterActionsPanel.h>
#include <mishmesh/applets/RepeaterSettingsApplet.h>
#include <mishmesh/applets/RepeaterRadioPanel.h>
#include <mishmesh/applets/RepeaterTelemetryApplet.h>
#include <mishmesh/applets/RepeaterNeighborsApplet.h>
#include <mishmesh/applets/RepeaterRegionsApplet.h>
#include <mishmesh/applets/RepeaterAclApplet.h>
#include <mishmesh/applets/RepeaterIdentityApplet.h>

using mishmesh::RemoteSettingsEngine;
using mishmesh::SettingFieldDef;

static const SettingFieldDef POS_DEFS[] = {
  {"Latitude",  "get lat", "set lat", SettingFieldDef::Float, 0, 0},
  {"Longitude", "get lon", "set lon", SettingFieldDef::Float, 0, 0},
};
static const SettingFieldDef READONLY_DEFS[] = {
  {"Version", "ver", nullptr, SettingFieldDef::ReadOnly, 0, 0},
};
static const SettingFieldDef LONGVAL_DEFS[] = {
  {"Public key", "get public.key", nullptr, SettingFieldDef::ReadOnly, 0, 0, nullptr, true},
};
// Mixed panel: one editable + one longValue ReadOnly. Used to test the pubkey modal
// in list mode (panels with editable fields stay in list mode, not text mode).
static const SettingFieldDef MIXED_DEFS[] = {
  {"Name",       "get name",       "set name", SettingFieldDef::Text, 0, 0},
  {"Public key", "get public.key", nullptr,    SettingFieldDef::ReadOnly, 0, 0, nullptr, true},
};

TEST(RSEngine, FetchesFieldsSequentially) {
  FakeContactsService svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  char staged[2][24] = {{0}}; bool dirty[2] = {false}, fetched[2] = {false};
  RemoteSettingsEngine e;
  e.configure(&svc, pk, POS_DEFS, 2, &staged[0][0], 24, dirty, fetched);

  e.beginFetch(0);
  EXPECT_EQ("get lat", svc.lastCli);                  // first get fired
  svc.simulateCliReply(pk, "> 37.7");
  e.poll(svc.cliSeq(), 1);
  EXPECT_STREQ("37.7", staged[0]);                    // parsed (leading "> " stripped)
  EXPECT_EQ("get lon", svc.lastCli);                  // advanced to next
  svc.simulateCliReply(pk, "> -122.4");
  e.poll(svc.cliSeq(), 2);
  EXPECT_STREQ("-122.4", staged[1]);
  EXPECT_EQ(RemoteSettingsEngine::Phase::Ready, e.phase());
  EXPECT_TRUE(fetched[0]); EXPECT_TRUE(fetched[1]);
}

TEST(RSEngine, SkipsNullGetCmdOnFetch) {
  FakeContactsService svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  const SettingFieldDef defs[] = {
    {"Admin password", nullptr, "password", SettingFieldDef::Text, 0, 0},   // write-only
  };
  char staged[1][24] = {{0}}; bool dirty[1] = {false}, fetched[1] = {false};
  RemoteSettingsEngine e;
  e.configure(&svc, pk, defs, 1, &staged[0][0], 24, dirty, fetched);
  e.beginFetch(0);
  EXPECT_EQ(RemoteSettingsEngine::Phase::Ready, e.phase());   // nothing to fetch
  EXPECT_TRUE(fetched[0]);                                    // marked done, value empty
  EXPECT_STREQ("", staged[0]);
}

TEST(RSEngine, SavesDirtyFieldsAndClearsDirty) {
  FakeContactsService svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  char staged[2][24] = {{0}}; bool dirty[2] = {false}, fetched[2] = {true, true};
  strcpy(staged[0], "37.8"); dirty[0] = true;   // only lat edited
  RemoteSettingsEngine e;
  e.configure(&svc, pk, POS_DEFS, 2, &staged[0][0], 24, dirty, fetched);

  e.beginSave(10);
  EXPECT_EQ("set lat 37.8", svc.lastCli);
  svc.simulateCliReply(pk, "OK");
  e.poll(svc.cliSeq(), 11);
  EXPECT_FALSE(dirty[0]);
  EXPECT_EQ(RemoteSettingsEngine::Phase::Ready, e.phase());
}

TEST(RSEngine, SaveHaltsOnFirstError) {
  FakeContactsService svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  char staged[2][24] = {{0}}; bool dirty[2] = {true, true}, fetched[2] = {true, true};
  strcpy(staged[0], "999"); strcpy(staged[1], "-1");
  RemoteSettingsEngine e;
  e.configure(&svc, pk, POS_DEFS, 2, &staged[0][0], 24, dirty, fetched);

  e.beginSave(10);
  svc.simulateCliReply(pk, "Error, bad value");
  e.poll(svc.cliSeq(), 11);
  EXPECT_EQ(RemoteSettingsEngine::Phase::Error, e.phase());
  EXPECT_EQ(0, e.errorIndex());
  EXPECT_TRUE(dirty[1]);   // second field not attempted
}

static const SettingFieldDef PANEL_DEFS[] = {
  {"Latitude",  "get lat", "set lat", SettingFieldDef::Float, 0, 0},
  {"Longitude", "get lon", "set lon", SettingFieldDef::Float, 0, 0},
};
static const SettingFieldDef TOGGLE_DEFS[] = {
  {"Repeat mode", "get repeat", "set repeat", SettingFieldDef::Toggle, 0, 0},
};
static const SettingFieldDef TEXT_DEFS[] = {
  {"Name", "get name", "set name", SettingFieldDef::Text, 0, 0},
};

TEST(RSettingsPanel, FetchRendersValues) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsPanel().setTarget(pk, "Rep1");
  mishmesh::repeaterSettingsPanel().setModel(PANEL_DEFS, 2, "Position");
  host.setRoot(&mishmesh::repeaterSettingsPanel());
  host.loop(1);                          // onStart fetches -> fires "get lat"; render cadence 150ms
  svc.simulateCliReply(pk, "> 37.7");
  host.loop(200);                        // past the 150ms cadence: polls lat, fires "get lon"
  svc.simulateCliReply(pk, "> -122.4");
  host.loop(400);                        // polls lon
  EXPECT_STREQ("37.7", mishmesh::repeaterSettingsPanel().valueForTest(0));
  EXPECT_STREQ("-122.4", mishmesh::repeaterSettingsPanel().valueForTest(1));
}

TEST(RSettingsPanel, SaveRowExistsForEditablePanel) {
  // Save is now the FormView button at focus _n (not a list row).
  // fieldCountForTest() == _n (fields only); hasButtonForTest() == true when editable.
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsPanel().setTarget(pk, "Rep1");
  mishmesh::repeaterSettingsPanel().setModel(PANEL_DEFS, 2, "Position");
  host.setRoot(&mishmesh::repeaterSettingsPanel());
  host.loop(1);
  EXPECT_EQ(2, mishmesh::repeaterSettingsPanel().fieldCountForTest());  // fields only
  EXPECT_TRUE(mishmesh::repeaterSettingsPanel().hasButtonForTest());    // Save button present
  EXPECT_TRUE(mishmesh::repeaterSettingsPanel().hasEditableForTest());
}

TEST(RSettingsPanel, ToggleEditMarksDirtyAndSaveFires) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsPanel().setTarget(pk, "Rep1");
  mishmesh::repeaterSettingsPanel().setModel(TOGGLE_DEFS, 1, "Repeat");
  host.setRoot(&mishmesh::repeaterSettingsPanel());
  host.loop(1);                          // fetches -> fires "get repeat"
  svc.simulateCliReply(pk, "> off");
  host.loop(200);                        // polls the reply: fetched value = off, phase Ready
  EXPECT_STREQ("off", mishmesh::repeaterSettingsPanel().valueForTest(0));

  host.dispatch(mishmesh::InputEvent::Select);   // row 0 = toggle -> flips to on, dirty (requestRender)
  host.loop(300);
  EXPECT_STREQ("on", mishmesh::repeaterSettingsPanel().valueForTest(0));
  EXPECT_TRUE(mishmesh::repeaterSettingsPanel().anyDirtyForTest());

  host.dispatch(mishmesh::InputEvent::NavDown); // selection moves to Save row (index _n=1)
  host.dispatch(mishmesh::InputEvent::Select);  // -> beginSave fires the set synchronously
  EXPECT_EQ("set repeat on", svc.lastCli);
  host.loop(400);                        // saving render cadence 150ms
  svc.simulateCliReply(pk, "OK");
  host.loop(700);                        // past cadence: polls the OK, clears dirty
  EXPECT_FALSE(mishmesh::repeaterSettingsPanel().anyDirtyForTest());
}

TEST(RSettingsPanel, KeypadConfirmResumesPolling) {
  // Regression: onForeground() used to check !_editSubmitted, so a confirmed edit
  // (OK path) left _phase==Editing and blocked all subsequent engine polling.
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsPanel().setTarget(pk, "Rep1");
  mishmesh::repeaterSettingsPanel().setModel(TEXT_DEFS, 1, "General");
  host.setRoot(&mishmesh::repeaterSettingsPanel());
  host.loop(1);                          // onStart fetches -> fires "get name"
  svc.simulateCliReply(pk, "> old");
  host.loop(200);                        // polls reply: fetched value = "old", phase Ready
  EXPECT_STREQ("old", mishmesh::repeaterSettingsPanel().valueForTest(0));

  // Select field row 0 -> pushes keypad; panel enters Editing phase
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_TRUE(mishmesh::repeaterSettingsPanel().editingForTest());

  // Navigate keypad to OK cell (row 3, col 3): keypad starts at focus (0,1)
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);   // now at (3,1)
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.dispatch(mishmesh::InputEvent::NavRight);  // now at (3,3) = OK
  // Confirm: onEditDone fires (s_dirty[0]=true), keypad pops, onForeground resets phase
  host.dispatch(mishmesh::InputEvent::Select);

  // With the bug, _phase stayed Editing here; with the fix it returns to List
  EXPECT_FALSE(mishmesh::repeaterSettingsPanel().editingForTest());
  // onEditDone staged the (unchanged) value as dirty
  EXPECT_TRUE(mishmesh::repeaterSettingsPanel().anyDirtyForTest());

  // Navigate to Save row and commit -> proves polling can resume
  host.dispatch(mishmesh::InputEvent::NavDown);   // field row 0 -> Save row (index _n=1)
  host.dispatch(mishmesh::InputEvent::Select);    // beginSave -> fires "set name old"
  EXPECT_EQ("set name old", svc.lastCli);

  svc.simulateCliReply(pk, "OK");
  host.loop(700);                        // past the 150ms render cadence: polls OK, clears dirty
  EXPECT_FALSE(mishmesh::repeaterSettingsPanel().anyDirtyForTest());
}

TEST(RActions, SendAdvertFiresCli) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterActionsPanel().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterActionsPanel());
  host.loop(1);
  // Row 0 = Send advert.
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);
  EXPECT_EQ("advert", svc.lastCli);
}

TEST(RActions, AdvertNoInstantToastAwaitReply) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterActionsPanel().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterActionsPanel());
  host.loop(1);
  // Fire advert (row 0).
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ("advert", svc.lastCli);
  // No instant toast - panel must be Busy awaiting the reply.
  EXPECT_STREQ("", host.toastForTest());
  EXPECT_EQ(mishmesh::RepeaterActionsPanel::Phase::Busy,
            mishmesh::repeaterActionsPanel().phaseForTest());
  // Simulate the repeater's reply.
  svc.simulateCliReply(pk, "OK - Advert sent");
  host.loop(150);
  // Toast now shows the actual reply text.
  EXPECT_STREQ("OK - Advert sent", host.toastForTest());
  EXPECT_EQ(mishmesh::RepeaterActionsPanel::Phase::Idle,
            mishmesh::repeaterActionsPanel().phaseForTest());
}

TEST(RActions, SyncClockNoInstantToastAwaitReply) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterActionsPanel().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterActionsPanel());
  host.loop(1);
  // Navigate to row 1 = Sync clock.
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(0, strncmp(svc.lastCli.c_str(), "time ", 5));   // command sent
  // No instant toast.
  EXPECT_STREQ("", host.toastForTest());
  EXPECT_EQ(mishmesh::RepeaterActionsPanel::Phase::Busy,
            mishmesh::repeaterActionsPanel().phaseForTest());
  // Simulate the repeater's reply.
  svc.simulateCliReply(pk, "OK - clock set");
  host.loop(150);
  EXPECT_STREQ("OK - clock set", host.toastForTest());
  EXPECT_EQ(mishmesh::RepeaterActionsPanel::Phase::Idle,
            mishmesh::repeaterActionsPanel().phaseForTest());
}

TEST(RActions, AdvertTimeout) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterActionsPanel().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterActionsPanel());
  host.loop(1);
  host.dispatch(mishmesh::InputEvent::Select);   // fire advert at t=1ms
  EXPECT_STREQ("", host.toastForTest());         // no instant toast
  // Advance past the 8s timeout (PendingRequest::timeoutMs = 8000).
  host.loop(9002);
  EXPECT_STREQ("[no response]", host.toastForTest());
  EXPECT_EQ(mishmesh::RepeaterActionsPanel::Phase::Idle,
            mishmesh::repeaterActionsPanel().phaseForTest());
}

TEST(RActions, RebootAsksConfirmThenFires) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterActionsPanel().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterActionsPanel());
  host.loop(1);
  // Rows: 0 Send advert, 1 Sync clock, 2 Reboot.
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);   // opens confirm, does NOT fire yet
  host.loop(2);
  EXPECT_NE("reboot", svc.lastCli);
  host.dispatch(mishmesh::InputEvent::Select);   // confirm defaults to Confirm -> fires
  host.loop(3);
  EXPECT_EQ("reboot", svc.lastCli);
}

TEST(RSettingsHub, RoutesFieldPanel) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterSettingsApplet());
  host.loop(1);
  host.dispatch(mishmesh::InputEvent::Select);   // row 0 = Public info -> pushes the panel
  host.loop(2);
  EXPECT_EQ(2, host.depth());
}

TEST(RSEngine, SaveSuccessMatchesCustomOkPrefix) {
  FakeContactsService svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  const SettingFieldDef defs[] = {
    {"Admin pass", nullptr, "password", SettingFieldDef::Text, 0, 0, "password now:"},
  };
  char staged[1][24] = {{0}}; bool dirty[1] = {true}, fetched[1] = {true};
  strcpy(staged[0], "secret");
  RemoteSettingsEngine e;
  e.configure(&svc, pk, defs, 1, &staged[0][0], 24, dirty, fetched);
  e.beginSave(10);
  EXPECT_EQ("password secret", svc.lastCli);
  svc.simulateCliReply(pk, "password now: secret");   // firmware's actual reply
  e.poll(svc.cliSeq(), 11);
  EXPECT_FALSE(dirty[0]);                              // treated as success
  EXPECT_EQ(RemoteSettingsEngine::Phase::Ready, e.phase());
}

TEST(RSettingsHub, RoutesRadioPanel) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterSettingsApplet());
  host.loop(1);
  host.dispatch(mishmesh::InputEvent::NavDown);   // row 1 = Radio
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);
  EXPECT_EQ(2, host.depth());                      // hub + radio panel
}

TEST(RSettingsHub, RoutesTelemetryAndNeighbors) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterSettingsApplet());
  host.loop(1);
  // Find the Telemetry row by navigating; assert it pushes (depth 2), then back, then Neighbors.
  // Telemetry is catalog index 10, Neighbors newly inserted at 11 (before Version).
  for (int i = 0; i < 10; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);      // Telemetry
  host.loop(2);
  EXPECT_EQ(2, host.depth());
}

TEST(RSettingsHub, RoutesRegions) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterSettingsApplet());
  host.loop(1);
  // Regions is catalog index 9 (Public info, Radio, Owner, Position, Advert, Repeat,
  // Passwords, Access control, Identity key, Regions). Navigate + select; assert depth 2.
  for (int i = 0; i < 9; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);
  EXPECT_EQ(2, host.depth());
}

TEST(RSettingsHub, RoutesAcl) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterSettingsApplet());
  host.loop(1);
  // Access control is catalog index 7: navigate 7 rows down then select.
  for (int i = 0; i < 7; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);
  EXPECT_EQ(2, host.depth());   // hub + ACL applet
}

TEST(RSettingsHub, RoutesIdentity) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterSettingsApplet());
  host.loop(1);
  // Change identity is catalog index 8: navigate 8 rows down then select.
  for (int i = 0; i < 8; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);
  EXPECT_EQ(2, host.depth());   // hub + identity applet
}

TEST(RSettingsPanel, ReadOnlyOnlyPanelRendersAsText) {
  // VERSION panel: all ReadOnly -> hasEditableForTest() false; panel uses text view, no Save row.
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsPanel().setTarget(pk, "Rep1");
  mishmesh::repeaterSettingsPanel().setModel(READONLY_DEFS, 1, "Version");
  host.setRoot(&mishmesh::repeaterSettingsPanel());
  host.loop(1);
  EXPECT_FALSE(mishmesh::repeaterSettingsPanel().hasEditableForTest());
  EXPECT_EQ(1, mishmesh::repeaterSettingsPanel().fieldCountForTest());  // 1 field
  EXPECT_FALSE(mishmesh::repeaterSettingsPanel().hasButtonForTest());   // no Save button
  // NavDown/Select scroll the text view - no CLI command fired, no save possible.
  svc.lastCli = "";
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);
  EXPECT_EQ("", svc.lastCli);
  // After fetch completes, the text view shows the fetched value.
  svc.simulateCliReply(pk, "> v1.2.3");
  host.loop(200);
  EXPECT_STREQ("v1.2.3", mishmesh::repeaterSettingsPanel().textViewLineForTest(0));
}

TEST(RSettingsPanel, LongValueSelectOpenModal) {
  // A ReadOnly+longValue field inside an editable panel stays in list mode -> Select opens modal.
  // (A panel with only ReadOnly fields uses text mode instead, where no modal is needed.)
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsPanel().setTarget(pk, "Rep1");
  mishmesh::repeaterSettingsPanel().setModel(MIXED_DEFS, 2, "Public info");
  host.setRoot(&mishmesh::repeaterSettingsPanel());
  host.loop(1);
  svc.simulateCliReply(pk, "> RepNode");   // fetch "get name" (field 0)
  host.loop(200);
  // Simulate fetch returning a 64-char hex pubkey value (field 1).
  svc.simulateCliReply(pk, "> AABBCCDDEEFF00112233445566778899AABBCCDDEEFF00112233445566778899AA");
  host.loop(400);
  EXPECT_FALSE(mishmesh::repeaterSettingsPanel().inModalForTest());
  // NavDown from row 0 (Name) to row 1 (Public key), then Select -> modal.
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_TRUE(mishmesh::repeaterSettingsPanel().inModalForTest());
  // Back closes the modal.
  host.dispatch(mishmesh::InputEvent::Back);
  EXPECT_FALSE(mishmesh::repeaterSettingsPanel().inModalForTest());
}

TEST(RSettingsPanel, LongValueListShowsTruncated) {
  // The list value column for a longValue field shows at most 6 chars + "...".
  // Uses a mixed panel (editable Name + ReadOnly Public key) so list mode is active.
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterSettingsPanel().setTarget(pk, "Rep1");
  mishmesh::repeaterSettingsPanel().setModel(MIXED_DEFS, 2, "Public info");
  host.setRoot(&mishmesh::repeaterSettingsPanel());
  host.loop(1);
  svc.simulateCliReply(pk, "> RepNode");         // Name field (index 0)
  host.loop(200);
  svc.simulateCliReply(pk, "> AABBCCDDEEFF001122");  // Public key field (index 1)
  host.loop(400);
  // Raw value is stored in full by the engine.
  const char* raw = mishmesh::repeaterSettingsPanel().valueForTest(1);
  EXPECT_STRNE("", raw);
  EXPECT_TRUE(strncmp(raw, "AABB", 4) == 0);
  // Display value is truncated to 6 chars + "..." for longValue fields.
  const char* display = mishmesh::repeaterSettingsPanel().displayValueForTest(1);
  EXPECT_STREQ("AABBCC...", display);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
