#include <gtest/gtest.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/applets/AdvertApplet.h>
#include <mishmesh/core/AppletHost.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
// AppServices that does NOT override sendAdvert, to assert the safe default.
class BareApp : public AppServices {
public:
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
};

// AppServices that records sendAdvert() calls.
class FakeAdvertApp : public AppServices {
public:
  int calls = 0; bool lastFlood = false; bool ret = true;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool sendAdvert(bool flood) override { calls++; lastFlood = flood; return ret; }
};
}  // namespace

TEST(AppServicesAdvert, DefaultIsSafeNoop) {
  BareApp a;
  EXPECT_FALSE(a.sendAdvert(false));   // default no-op returns false, no crash
  EXPECT_FALSE(a.sendAdvert(true));
}

TEST(AdvertApplet, SelectOnFirstRowSendsZeroHop) {
  FakeAdvertApp app;
  AppletContext ctx; ctx.app = &app;       // no host -> toast skipped, send still runs
  AdvertApplet applet;
  applet.onStart(ctx);
  EXPECT_TRUE(applet.onInput(InputEvent::Select));  // consumed
  EXPECT_EQ(1, app.calls);
  EXPECT_FALSE(app.lastFlood);             // row 0 == zero hop
}

TEST(AdvertApplet, NavDownThenSelectSendsFlood) {
  FakeAdvertApp app;
  AppletContext ctx; ctx.app = &app;
  AdvertApplet applet;
  applet.onStart(ctx);
  EXPECT_TRUE(applet.onInput(InputEvent::NavDown));  // ListMenu consumes nav
  EXPECT_TRUE(applet.onInput(InputEvent::Select));
  EXPECT_EQ(1, app.calls);
  EXPECT_TRUE(app.lastFlood);              // row 1 == flood routed
}

TEST(AdvertApplet, BackBubbles) {
  FakeAdvertApp app;
  AppletContext ctx; ctx.app = &app;
  AdvertApplet applet;
  applet.onStart(ctx);
  EXPECT_FALSE(applet.onInput(InputEvent::Back));    // not consumed -> host pops
  EXPECT_EQ(0, app.calls);
}

TEST(AdvertApplet, ModelLabels) {
  AdvertApplet applet;
  EXPECT_EQ(2, applet.count());
  EXPECT_STREQ("Zero hop", applet.label(0));
  EXPECT_STREQ("Flood routed", applet.label(1));
}

TEST(AdvertApplet, RendersWithoutCrashing) {
  FakeDisplayDriver d;
  FakeAdvertApp app;
  AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);
  AdvertApplet applet;
  host.push(&applet);
  host.loop(0);
  EXPECT_FALSE(d.fills.empty());   // menu rows + hint drawn as pixel runs
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
