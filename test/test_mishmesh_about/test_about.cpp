#include <gtest/gtest.h>
#include <mishmesh/applets/AboutApplet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
#include <cstring>

using namespace mishmesh;

// AboutApplet only reads systemStats (for the version line) + battery.
struct FakeApp : AppServices {
  const char* nodeName() const override { return "me"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool systemStats(SystemStats& out) const override {
    out.firmwareVersion = "mishmesh-1.2.3";
    return true;
  }
};

TEST(AboutApplet, EncodesSupportUrlAsQr) {
  QrView qr;
  ASSERT_TRUE(qr.build(aboutApplet().qrTextForTest()));
  EXPECT_TRUE(qr.valid());
}

TEST(AboutApplet, QrTargetsKofiPage) {
  EXPECT_STREQ(aboutApplet().qrTextForTest(), "https://ko-fi.com/burak_can");
}

TEST(AboutApplet, RendersWithoutCrash) {
  FakeApp app; AppletContext ctx; ctx.app = &app;
  auto& a = aboutApplet(); a.onStart(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  a.onRender(c);
  SUCCEED();
}

TEST(AboutApplet, RendersWhenVersionUnavailable) {
  struct NoStats : FakeApp {
    bool systemStats(SystemStats&) const override { return false; }
  } app;
  AppletContext ctx; ctx.app = &app;
  auto& a = aboutApplet(); a.onStart(ctx);
  FakeDisplayDriver d; Canvas c(&d);
  a.onRender(c);
  SUCCEED();
}

TEST(AboutApplet, BackBubbles) {
  FakeApp app; AppletContext ctx; ctx.app = &app;
  auto& a = aboutApplet(); a.onStart(ctx);
  EXPECT_FALSE(a.onInput(InputEvent::Back));   // not consumed -> host pops
}

TEST(AboutApplet, RegisteredInAppMenuWithCoffeeIcon) {
  bool found = false;
  for (AppletRegistration* r = registeredApplets(); r; r = r->next) {
    if (r->applet == &aboutApplet()) {
      found = true;
      EXPECT_EQ(r->placement, Placement::AppMenu);
      EXPECT_STREQ(r->label, "About");
      EXPECT_EQ(r->icon, (uint16_t)Icon::Coffee);
    }
  }
  EXPECT_TRUE(found);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
