#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include "FakeDisplayDriver.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/RepeaterIdentityApplet.h>
#include <cstring>
#include <string>

using namespace mishmesh;

static const uint8_t kPub[6] = {'R', 'E', 'P', '0', '0', '1'};
// A 64-hex seed (valid): 64 chars of lowercase hex.
static const char* kGoodSeed = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
// A bad seed: wrong length.
static const char* kBadSeed  = "tooshort";

// Drive the applet from Menu through random-generate, reaching Show.
static void reachShow(AppletHost& host, RepeaterIdentityApplet& ap) {
  host.loop(1);
  // Row 0 is "Generate random" (already selected after reset)
  host.dispatch(InputEvent::Select);   // -> doGenerate() -> Show
  host.loop(2);
}

TEST(RepeaterIdentity, RandomGenerateShowsKey) {
  FakeContactsService svc; FakeDisplayDriver d;
  AppletContext ctx; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  auto& ap = repeaterIdentityApplet();
  ap.setTarget(kPub, "TestRep");
  host.setRoot(&ap);
  reachShow(host, ap);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Show, ap.phaseForTest());
  // fake fills with 128 'a' chars for null seed
  const char* key = ap.shownKeyForTest();
  ASSERT_NE(nullptr, key);
  EXPECT_EQ(128u, strlen(key));
  for (int i = 0; i < 128; i++) EXPECT_EQ('a', key[i]) << "at index " << i;
}

TEST(RepeaterIdentity, ConfirmFiresSetKey) {
  FakeContactsService svc; FakeDisplayDriver d;
  AppletContext ctx; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  auto& ap = repeaterIdentityApplet();
  ap.setTarget(kPub, "TestRep");
  host.setRoot(&ap);
  reachShow(host, ap);
  // Select in Show -> Confirm phase
  host.dispatch(InputEvent::Select);
  host.loop(3);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Confirm, ap.phaseForTest());
  // Confirm in ConfirmDialog (default sel = 1 = Confirm)
  host.dispatch(InputEvent::Select);
  host.loop(4);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Busy, ap.phaseForTest());
  // Check the CLI command: "set prv.key " + 128 'a' chars
  ASSERT_GE(svc.lastCli.size(), 12u + 128u);
  EXPECT_EQ("set prv.key ", svc.lastCli.substr(0, 12));
  for (int i = 12; i < 12 + 128; i++) EXPECT_EQ('a', svc.lastCli[i]) << "at index " << i;
}

TEST(RepeaterIdentity, OkReplyFiresRebootAndDone) {
  FakeContactsService svc; FakeDisplayDriver d;
  AppletContext ctx; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  auto& ap = repeaterIdentityApplet();
  ap.setTarget(kPub, "TestRep");
  host.setRoot(&ap);
  reachShow(host, ap);
  host.dispatch(InputEvent::Select);   // -> Confirm
  host.loop(3);
  host.dispatch(InputEvent::Select);   // confirm -> doSetKey() -> Busy
  host.loop(4);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Busy, ap.phaseForTest());
  // Simulate the firmware reply
  svc.simulateCliReply(kPub,
    "OK, reboot to apply! New pubkey: "
    "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
  // Advance time past the 150ms Busy re-render threshold
  host.loop(200);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Done, ap.phaseForTest());
  // reboot command must have been the last CLI sent
  EXPECT_EQ("reboot", svc.lastCli);
}

TEST(RepeaterIdentity, EnterSeedGoodPath) {
  FakeContactsService svc; FakeDisplayDriver d;
  AppletContext ctx; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  auto& ap = repeaterIdentityApplet();
  ap.setTarget(kPub, "TestRep");
  host.setRoot(&ap);
  host.loop(1);
  // Navigate to row 1 ("Enter seed") and select it -> push KeypadApplet
  host.dispatch(InputEvent::NavDown);
  host.loop(2);
  host.dispatch(InputEvent::Select);   // pushes KeypadApplet, phase = Enter
  host.loop(3);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Enter, ap.phaseForTest());
  // Simulate the user typing 64 hex chars and confirming
  RepeaterIdentityApplet::onSeedConfirm(&ap, kGoodSeed);
  host.pop();    // pops KeypadApplet -> triggers ap.onForeground()
  host.loop(4);  // renders Show
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Show, ap.phaseForTest());
  // fake fills 128 'b' chars for a valid seed
  const char* key = ap.shownKeyForTest();
  ASSERT_NE(nullptr, key);
  EXPECT_EQ(128u, strlen(key));
  for (int i = 0; i < 128; i++) EXPECT_EQ('b', key[i]) << "at index " << i;
  // Proceed to confirm and fire the command
  host.dispatch(InputEvent::Select);   // -> Confirm
  host.loop(5);
  host.dispatch(InputEvent::Select);   // confirm -> doSetKey()
  host.loop(6);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Busy, ap.phaseForTest());
  ASSERT_GE(svc.lastCli.size(), 12u + 128u);
  EXPECT_EQ("set prv.key ", svc.lastCli.substr(0, 12));
  for (int i = 12; i < 12 + 128; i++) EXPECT_EQ('b', svc.lastCli[i]) << "at index " << i;
}

TEST(RepeaterIdentity, BadSeedNoCliNoSetKey) {
  FakeContactsService svc; FakeDisplayDriver d;
  AppletContext ctx; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  auto& ap = repeaterIdentityApplet();
  ap.setTarget(kPub, "TestRep");
  host.setRoot(&ap);
  host.loop(1);
  host.dispatch(InputEvent::NavDown);
  host.loop(2);
  host.dispatch(InputEvent::Select);   // push KeypadApplet, phase = Enter
  host.loop(3);
  // Simulate bad seed (too short)
  RepeaterIdentityApplet::onSeedConfirm(&ap, kBadSeed);
  host.pop();    // -> onForeground() -> makeIdentityHex returns false -> Menu
  host.loop(4);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Menu, ap.phaseForTest());
  // No CLI command was ever sent
  EXPECT_TRUE(svc.lastCli.empty());
}

TEST(RepeaterIdentity, ConfirmCancelNoSetKey) {
  FakeContactsService svc; FakeDisplayDriver d;
  AppletContext ctx; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  auto& ap = repeaterIdentityApplet();
  ap.setTarget(kPub, "TestRep");
  host.setRoot(&ap);
  reachShow(host, ap);
  // Select -> Confirm phase
  host.dispatch(InputEvent::Select);
  host.loop(3);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Confirm, ap.phaseForTest());
  // Back -> Cancelled -> back to Show
  host.dispatch(InputEvent::Back);
  host.loop(4);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Show, ap.phaseForTest());
  // No CLI command fired
  EXPECT_TRUE(svc.lastCli.empty());
}

TEST(RepeaterIdentity, ErrorReplyNoRebootStaysMenu) {
  FakeContactsService svc; FakeDisplayDriver d;
  AppletContext ctx; ctx.contacts = &svc;
  AppletHost host(&d, ctx);
  auto& ap = repeaterIdentityApplet();
  ap.setTarget(kPub, "TestRep");
  host.setRoot(&ap);
  reachShow(host, ap);
  host.dispatch(InputEvent::Select);   // Show -> Confirm
  host.loop(3);
  host.dispatch(InputEvent::Select);   // confirm -> doSetKey() -> Busy
  host.loop(4);
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Busy, ap.phaseForTest());
  // Record the set-prv-key command that was just sent
  std::string setPrvCmd = svc.lastCli;
  ASSERT_GE(setPrvCmd.size(), 12u + 128u);
  EXPECT_EQ("set prv.key ", setPrvCmd.substr(0, 12));
  // Simulate firmware rejection
  svc.simulateCliReply(kPub, "Error, bad key");
  // Advance past Busy re-render threshold
  host.loop(200);
  // reboot must NOT have been sent
  EXPECT_EQ(setPrvCmd, svc.lastCli) << "reboot must not be sent on error reply";
  // Applet must return to Menu
  EXPECT_EQ(RepeaterIdentityApplet::Phase::Menu, ap.phaseForTest());
}

int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
