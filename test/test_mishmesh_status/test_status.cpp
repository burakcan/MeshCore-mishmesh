#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include <mishmesh/applets/StatusApplet.h>
#include <cstring>
#include <string>
#include "FakeDisplayDriver.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/RepeaterManageApplet.h>

TEST(FakeStatus, RequestRecordsAndReplyLatches) {
  FakeContactsService svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};

  EXPECT_TRUE(svc.requestStatus(pk));
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastStatusReq);
  uint32_t before = svc.statusSeq();

  mishmesh::RepeaterStatusView v{};
  v.valid = true; v.upTimeSecs = 7500; v.battMilliVolts = 4020;
  svc.simulateStatus(pk, v);
  EXPECT_NE(before, svc.statusSeq());

  mishmesh::RepeaterStatusView out{};
  ASSERT_TRUE(svc.latestStatus(pk, out));
  EXPECT_TRUE(out.valid);
  EXPECT_EQ(7500u, out.upTimeSecs);

  uint8_t other[6] = {'X','X','X','X','X','X'};
  EXPECT_FALSE(svc.latestStatus(other, out));   // reply was for a different contact

  svc.loginClockVal = 52320;
  EXPECT_EQ(52320u, svc.loginClock(pk));
}

TEST(FormatStatus, RendersCompactRowsWithinCap) {
  mishmesh::RepeaterStatusView s{};
  s.valid = true;
  s.upTimeSecs = 7500;          // 2h 05m
  s.battMilliVolts = 4020;      // 4.02V
  s.airTxSecs = 3720; s.airRxSecs = 300;   // 1h2m ; 5m
  s.lastRssi = -92; s.lastSnrX4 = 30;      // 7.5 dB
  s.noiseFloor = -110; s.txQueueLen = 0;
  s.packetsSent = 142; s.sentFlood = 120; s.sentDirect = 22;
  s.packetsRecv = 98; s.recvFlood = 80; s.recvDirect = 18;
  s.floodDups = 4; s.directDups = 1; s.recvErrors = 0; s.errEvents = 0;

  char lines[mishmesh::STATUS_MAX_LINES][mishmesh::STATUS_LINE_LEN];
  int n = mishmesh::formatRepeaterStatus(s, 52320 /*14:32*/, lines, mishmesh::STATUS_MAX_LINES);
  ASSERT_GT(n, 0);
  ASSERT_LE(n, mishmesh::STATUS_MAX_LINES);
  EXPECT_STREQ("Uptime: 2h 05m", lines[0]);
  EXPECT_STREQ("Battery: 4.02V", lines[1]);
  // Clock row present, formatted HH:MM.
  bool sawClock = false;
  for (int i = 0; i < n; i++) if (std::string(lines[i]) == "Clock: 14:32") sawClock = true;
  EXPECT_TRUE(sawClock);
}

TEST(FormatStatus, UnknownClockRendersDashes) {
  mishmesh::RepeaterStatusView s{}; s.valid = true;
  char lines[mishmesh::STATUS_MAX_LINES][mishmesh::STATUS_LINE_LEN];
  int n = mishmesh::formatRepeaterStatus(s, 0, lines, mishmesh::STATUS_MAX_LINES);
  bool sawDash = false;
  for (int i = 0; i < n; i++) if (std::string(lines[i]) == "Clock: --") sawDash = true;
  EXPECT_TRUE(sawDash);
}

TEST(FormatStatus, InvalidViewRendersNoData) {
  mishmesh::RepeaterStatusView s{};   // valid = false
  char lines[mishmesh::STATUS_MAX_LINES][mishmesh::STATUS_LINE_LEN];
  int n = mishmesh::formatRepeaterStatus(s, 0, lines, mishmesh::STATUS_MAX_LINES);
  ASSERT_EQ(1, n);
  EXPECT_STREQ("No data", lines[0]);
}

TEST(StatusApplet, OpenFiresRequest) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::statusApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::statusApplet());
  host.loop(1);
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastStatusReq);
}

TEST(StatusApplet, ReplyRendersRows) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::statusApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::statusApplet());
  host.loop(1);

  mishmesh::RepeaterStatusView v{}; v.valid = true; v.upTimeSecs = 7500; v.battMilliVolts = 4020;
  svc.simulateStatus(pk, v);
  host.loop(300);   // poll sees the seq bump, renders rows

  bool sawUptime = false;
  for (int i = 0; i < mishmesh::statusApplet().lineCountForTest(); i++)
    if (std::string(mishmesh::statusApplet().lineForTest(i)) == "Uptime: 2h 05m") sawUptime = true;
  EXPECT_TRUE(sawUptime);
}

TEST(StatusApplet, TimeoutRendersRetry) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::statusApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::statusApplet());
  host.loop(1);
  host.loop(25000);   // past the 20s timeout, under the 30s auto-off

  bool sawNoResp = false;
  for (int i = 0; i < mishmesh::statusApplet().lineCountForTest(); i++)
    if (std::string(mishmesh::statusApplet().lineForTest(i)) == "No response") sawNoResp = true;
  EXPECT_TRUE(sawNoResp);
}

// Verify that the embedded-mode onShow (called by RepeaterManageApplet when the
// Status tab becomes active) auto-fires the status request.
TEST(StatusApplet, OnShowFiresRequest) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::statusApplet().setTarget(pk, "Rep1");
  // Simulate the embedded activation path (hub calls onShow).
  mishmesh::statusApplet().onShow(ctx);
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastStatusReq);
}

// Opening the management hub auto-fires the status request (Status is the first tab).
TEST(StatusApplet, HubAutoRequestsOnOpen) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterManageApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterManageApplet());
  host.loop(1);
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastStatusReq);
}

// Opening the hub fires one status request; switching away and back does NOT re-request.
TEST(StatusApplet, TabSwitchBackDoesNotReRequest) {
  FakeContactsService svc;
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterManageApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterManageApplet());
  host.loop(1);
  int before = 0;
  for (const auto& c : svc.calls) if (c == "reqstatus") before++;
  EXPECT_EQ(1, before);

  // Switch to Cmd tab (NavRight) then back to Status (NavLeft).
  host.dispatch(mishmesh::InputEvent::NavRight);
  host.loop(2);
  host.dispatch(mishmesh::InputEvent::NavLeft);
  host.loop(3);
  int after = 0;
  for (const auto& c : svc.calls) if (c == "reqstatus") after++;
  EXPECT_EQ(1, after);   // still only one request - tab switch-back is silent
}

// Refresh (Select in embedded mode) fires a new request even after the auto-fetch.
TEST(StatusApplet, SelectInEmbeddedModeReFetches) {
  FakeContactsService svc;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::statusApplet().setTarget(pk, "Rep1");
  mishmesh::statusApplet().onShow(ctx);   // auto-fetch #1, _autoFetchDone = true
  int count1 = 0;
  for (const auto& c : svc.calls) if (c == "reqstatus") count1++;
  EXPECT_EQ(1, count1);
  // Select fires the refresh regardless of _autoFetchDone.
  mishmesh::statusApplet().onInput(mishmesh::InputEvent::Select);
  int count2 = 0;
  for (const auto& c : svc.calls) if (c == "reqstatus") count2++;
  EXPECT_GE(count2, 2);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
