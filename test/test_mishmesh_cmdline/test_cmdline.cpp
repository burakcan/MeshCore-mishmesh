#include <gtest/gtest.h>
#include <mishmesh/core/PendingRequest.h>
#include "FakeContactsService.h"
#include <cstring>
#include "FakeDisplayDriver.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/CommandLineApplet.h>
#include <mishmesh/applets/RepeaterManageApplet.h>
#include <mishmesh/applets/RepeaterSettingsApplet.h>
#include <mishmesh/applets/StatusApplet.h>

using mishmesh::PendingRequest;

TEST(PendingRequest, BeginEntersWaiting) {
  PendingRequest r;
  r.begin(5, 1000);
  EXPECT_TRUE(r.waiting());
  EXPECT_EQ(PendingRequest::State::Waiting, r.poll(5, 1000));  // no bump, no timeout
}

TEST(PendingRequest, SeqBumpBecomesReady) {
  PendingRequest r;
  r.begin(5, 1000);
  EXPECT_EQ(PendingRequest::State::Ready, r.poll(6, 1100));    // seq changed
  EXPECT_EQ(PendingRequest::State::Ready, r.poll(6, 1200));    // sticky
}

TEST(PendingRequest, TimeoutFires) {
  PendingRequest r;
  r.timeoutMs = 8000;
  r.begin(5, 1000);
  EXPECT_EQ(PendingRequest::State::Waiting, r.poll(5, 8000));  // 7000ms elapsed
  EXPECT_EQ(PendingRequest::State::TimedOut, r.poll(5, 9001)); // >8000ms elapsed
}

TEST(PendingRequest, RearmKeepsWaitingButKeepsTimeoutClock) {
  PendingRequest r;
  r.timeoutMs = 8000;
  r.begin(5, 1000);
  ASSERT_EQ(PendingRequest::State::Ready, r.poll(6, 1100));    // a bump (for another contact)
  r.rearm(6);                                                  // consumer keeps waiting
  EXPECT_TRUE(r.waiting());
  EXPECT_EQ(PendingRequest::State::Waiting, r.poll(6, 2000));  // no further bump
  EXPECT_EQ(PendingRequest::State::TimedOut, r.poll(6, 9001)); // original clock still counts
}

TEST(PendingRequest, ResetReturnsToIdle) {
  PendingRequest r;
  r.begin(5, 1000);
  r.reset();
  EXPECT_FALSE(r.waiting());
  EXPECT_EQ(PendingRequest::State::Idle, r.poll(9, 9999));     // Idle is inert
}

TEST(FakeCli, SendRecordsAndReplyLatches) {
  FakeContactsService svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};

  EXPECT_TRUE(svc.sendCliCommand(pk, "ver"));
  EXPECT_EQ("ver", svc.lastCli);
  uint32_t before = svc.cliSeq();

  svc.simulateCliReply(pk, "v1.16.0");
  EXPECT_NE(before, svc.cliSeq());

  bool ok = false; const char* resp = nullptr;
  ASSERT_TRUE(svc.cliResult(pk, before, ok, resp));
  EXPECT_TRUE(ok);
  EXPECT_STREQ("v1.16.0", resp);

  uint8_t other[6] = {'X','X','X','X','X','X'};
  EXPECT_FALSE(svc.cliResult(other, before, ok, resp));   // reply was for a different contact
}

TEST(CommandLine, SubmitFiresCliAndAppendsPrompt) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::commandLineApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::commandLineApplet());
  host.loop(1);

  mishmesh::commandLineApplet().submitCommand("ver");
  EXPECT_EQ("ver", svc.lastCli);
  // The typed command echoes into the log as "> ver".
  bool sawPrompt = false;
  for (int i = 0; i < mishmesh::commandLineApplet().logCountForTest(); i++)
    if (std::string(mishmesh::commandLineApplet().logLineForTest(i)) == "> ver") sawPrompt = true;
  EXPECT_TRUE(sawPrompt);
}

TEST(CommandLine, ReplyAppendsToLog) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::commandLineApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::commandLineApplet());
  host.loop(1);

  mishmesh::commandLineApplet().submitCommand("get radio");
  svc.simulateCliReply(pk, "> 910.5,250,10,5");
  host.loop(100);   // onRender polls, sees the seq bump, appends the reply

  bool sawReply = false;
  for (int i = 0; i < mishmesh::commandLineApplet().logCountForTest(); i++)
    if (std::string(mishmesh::commandLineApplet().logLineForTest(i)) == "> 910.5,250,10,5") sawReply = true;
  EXPECT_TRUE(sawReply);
}

TEST(CommandLine, MultilineReplySplitsIntoLines) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::commandLineApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::commandLineApplet());
  host.loop(1);

  mishmesh::commandLineApplet().submitCommand("neighbors");
  svc.simulateCliReply(pk, "AABBCCDD:120:10\nEEFF0011:45:8");
  host.loop(100);

  int hits = 0;
  for (int i = 0; i < mishmesh::commandLineApplet().logCountForTest(); i++) {
    std::string l = mishmesh::commandLineApplet().logLineForTest(i);
    if (l == "AABBCCDD:120:10" || l == "EEFF0011:45:8") hits++;
  }
  EXPECT_EQ(2, hits);
}

TEST(CommandLine, TimeoutAppendsNoResponse) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::commandLineApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::commandLineApplet());
  host.loop(1);

  mishmesh::commandLineApplet().submitCommand("ver");   // no reply simulated
  host.loop(25000);   // well past the 20s CLI timeout (40s would trigger the 30s auto-off)

  bool sawTimeout = false;
  for (int i = 0; i < mishmesh::commandLineApplet().logCountForTest(); i++)
    if (std::string(mishmesh::commandLineApplet().logLineForTest(i)) == "[no response]") sawTimeout = true;
  EXPECT_TRUE(sawTimeout);
}

// --- RepeaterHub tab-model tests ---
// The hub is a TabBar host (Status / Cmd / Settings). No push on tab switch;
// drill-ins (keypad from CLI, panels from Settings) still push normally.

TEST(RepeaterHub, OpensOnStatusTabAndFiresRequest) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterManageApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterManageApplet());
  host.loop(1);

  // Hub opens on Status tab (index 0) and auto-fires the status request.
  EXPECT_EQ(0, mishmesh::repeaterManageApplet().selectedTabForTest());
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastStatusReq);
  EXPECT_EQ(1, host.depth());   // hub is the only applet on the stack
}

TEST(RepeaterHub, NavRightSwitchesTabsWithoutPush) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterManageApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterManageApplet());
  host.loop(1);

  // NavRight once -> Command line tab.
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.loop(2);
  EXPECT_EQ(1, mishmesh::repeaterManageApplet().selectedTabForTest());
  EXPECT_EQ(1, host.depth());   // still just the hub

  // NavRight again -> Settings tab.
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.loop(3);
  EXPECT_EQ(2, mishmesh::repeaterManageApplet().selectedTabForTest());
  EXPECT_EQ(1, host.depth());   // still just the hub
}

TEST(RepeaterHub, SettingsTabSelectPushesPanel) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterManageApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterManageApplet());
  host.loop(1);

  // Switch to Settings tab (tab 2).
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.loop(2);
  EXPECT_EQ(2, mishmesh::repeaterManageApplet().selectedTabForTest());
  EXPECT_EQ(1, host.depth());

  // Select the first catalog row (Public info) - should push the settings panel.
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(3);
  EXPECT_EQ(2, host.depth());   // hub + panel
}

TEST(RepeaterHub, StatusRefreshButtonRefires) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterManageApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterManageApplet());
  host.loop(1);

  // Opening fires the first request.
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastStatusReq);
  size_t reqsBefore = svc.calls.size();

  // Select always re-requests (Refresh is the header; no focus toggle needed).
  host.dispatch(mishmesh::InputEvent::Select);
  host.loop(2);

  bool sawNewReq = false;
  for (size_t i = reqsBefore; i < svc.calls.size(); i++)
    if (svc.calls[i] == "reqstatus") sawNewReq = true;
  EXPECT_TRUE(sawNewReq);
}

TEST(CommandLine, SelectWhilePendingDoesNotOpenKeypad) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::commandLineApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::commandLineApplet());
  host.loop(1);

  mishmesh::commandLineApplet().submitCommand("ver");   // now pending, no reply yet
  host.dispatch(mishmesh::InputEvent::Select);          // must be swallowed, not open keypad
  host.loop(2);
  EXPECT_EQ(1, host.depth());                            // keypad NOT pushed
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
