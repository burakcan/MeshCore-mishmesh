#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/applets/HomeApplet.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {

struct FakeApp : AppServices {
  bool ble = false, conn = false, gps = false, fix = false;
  const char* nodeName() const override { return "alice"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 13 * 3600 + 37 * 60; }
  bool bleSupported() const override { return true; }
  bool bleEnabled() const override { return ble; }
  bool bleConnected() const override { return conn; }
  bool gpsSupported() const override { return true; }
  bool gpsEnabled() const override { return gps; }
  bool gpsHasFix() const override { return fix; }
};

struct StubApplet : Applet {
  StubApplet() : Applet("stub") {}
  int onRender(Canvas&) override { return 1000; }
};

StubApplet leftTarget, rightTarget;
AppletRegistration regL{&leftTarget, "Contacts", 0, Placement::AppMenu, 1, nullptr};
AppletRegistration regR{&rightTarget, "Messages", 0, Placement::AppMenu, 0, nullptr};

void primeRegistry() {
  resetRegistry();
  registerApplet(&regL);
  registerApplet(&regR);
  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);
}

}  // namespace

TEST(HomeApplet, NavLeftAndRightPushConfiguredApplets) {
  primeRegistry();
  FakeDisplayDriver d;
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);
  HomeApplet home;
  host.setRoot(&home);

  host.dispatch(InputEvent::NavRight);
  EXPECT_EQ(&rightTarget, host.foreground());
  host.pop();
  host.dispatch(InputEvent::NavLeft);
  EXPECT_EQ(&leftTarget, host.foreground());
}

TEST(HomeApplet, NavDownOpensDrawerWithoutPushing) {
  primeRegistry();
  FakeDisplayDriver d;
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);
  HomeApplet home;
  host.setRoot(&home);

  host.dispatch(InputEvent::NavDown);
  host.loop(0);                               // one render: _h goes 0 -> 18
  EXPECT_EQ(1, host.depth());                 // drawer is an overlay, not a push
  EXPECT_TRUE(home.drawerForTest().isOpen());

  // While open, gestures go to the drawer, not to shortcuts.
  host.dispatch(InputEvent::NavRight);
  EXPECT_EQ(1, host.depth());
  EXPECT_EQ(1, home.drawerForTest().selected());

  host.dispatch(InputEvent::Back);            // close instead of bubbling
  EXPECT_TRUE(home.drawerForTest().isOpen()); // _open=false but _h=18 -> slide-out engaged
  for (uint32_t t = 1000; t <= 1400; t += 33) host.loop(t);
  EXPECT_FALSE(home.drawerForTest().isOpen());
  EXPECT_EQ(1, host.depth());
}

TEST(HomeApplet, DrawerClosesWhenHomeLosesForeground) {
  primeRegistry();
  FakeDisplayDriver d;
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);
  StubApplet other;
  HomeApplet home;
  host.setRoot(&home);
  host.dispatch(InputEvent::NavDown);
  EXPECT_TRUE(home.drawerForTest().isOpen());
  host.push(&other);                           // e.g. notification applet
  EXPECT_FALSE(home.drawerForTest().isOpen());
}

TEST(HomeApplet, GpsFixDrawsInvertedChip) {
  primeRegistry();
  FakeApp app;
  app.gps = true;

  // Render once searching, once with a fix: the fix adds exactly the chip fill.
  size_t fills[2];
  for (int pass = 0; pass < 2; pass++) {
    app.fix = pass == 1;
    FakeDisplayDriver d;
    AppletContext ctx; ctx.app = &app;
    AppletHost host(&d, ctx);
    HomeApplet home;
    host.setRoot(&home);
    host.loop(0);
    fills[pass] = d.fills.size();
  }
  EXPECT_GT(fills[1], fills[0]);
}

TEST(HomeApplet, HintLabelTruncatesToFourChars) {
  char out[5];
  HomeApplet::hintLabel("Contacts", out);      // hint = plain 4-char prefix
  EXPECT_STREQ("Cont", out);
  HomeApplet::hintLabel("2048", out);
  EXPECT_STREQ("2048", out);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
