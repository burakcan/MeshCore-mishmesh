#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/applets/HomeApplet.h>
#include "FakeDisplayDriver.h"

#include <string>

using namespace mishmesh;

namespace {

class FakeApp : public AppServices {
public:
  std::string name = "node";
  uint16_t mv = 4000;
  uint32_t epoch = 0;
  bool conn = false;
  const char* nodeName() const override { return name.c_str(); }
  uint16_t batteryMillivolts() const override { return mv; }
  uint32_t epochSeconds() const override { return epoch; }
};

class StubApplet : public Applet {
public:
  StubApplet() : Applet("stub") {}
  int onRender(Canvas&) override { return 1000; }
};

}  // namespace

TEST(HomeApplet, RendersStatusAndClock) {
  FakeDisplayDriver d;
  FakeApp app;
  app.name = "alice";
  app.epoch = 13 * 3600 + 37 * 60;   // 13:37:00 UTC
  AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);

  HomeApplet home;
  host.setRoot(&home);
  host.loop(0);

  EXPECT_FALSE(d.fills.empty());   // status bar + clock rasterised as pixels
}

TEST(HomeApplet, SelectPushesTheMenu) {
  FakeDisplayDriver d;
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);

  StubApplet menu;
  HomeApplet home;
  home.setMenu(&menu);
  host.setRoot(&home);

  host.dispatch(InputEvent::Select);
  EXPECT_EQ(2, host.depth());
  EXPECT_EQ(&menu, host.foreground());
}

TEST(HomeApplet, SelectWithoutMenuStaysHome) {
  FakeDisplayDriver d;
  FakeApp app;
  AppletContext ctx; ctx.app = &app;
  AppletHost host(&d, ctx);

  HomeApplet home;
  host.setRoot(&home);
  host.dispatch(InputEvent::Select);   // no menu wired
  EXPECT_EQ(1, host.depth());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
