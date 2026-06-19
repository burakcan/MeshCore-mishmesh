#include <gtest/gtest.h>
#include <mishmesh/core/Applet.h>

using namespace mishmesh;

namespace {
// Minimal concrete applet for testing the base contract.
class RecordingApplet : public Applet {
public:
  int started = 0, foreground = 0, background = 0, stopped = 0, rendered = 0;
  RecordingApplet() : Applet("rec") {}
  void onStart(AppletContext&) override { started++; }
  void onForeground() override { foreground++; }
  void onBackground() override { background++; }
  void onStop() override { stopped++; }
  int onRender(Canvas&) override { rendered++; return 1000; }
};
}  // namespace

TEST(Applet, ReportsName) {
  RecordingApplet a;
  EXPECT_STREQ("rec", a.name());
}

TEST(Applet, DefaultOnInputDoesNotConsume) {
  RecordingApplet a;
  EXPECT_FALSE(a.onInput(InputEvent::Select));
  EXPECT_FALSE(a.onInput(InputEvent::Back));
}

namespace {
// Minimal concrete AppServices to pin the companion-agnostic GPS defaults.
struct BareServices : mishmesh::AppServices {
  const char* nodeName() const override { return ""; }
  uint16_t batteryMillivolts() const override { return 0; }
  uint32_t epochSeconds() const override { return 0; }
};
}  // namespace

TEST(AppServices, GpsDefaultsAreOffAndInert) {
  BareServices s;
  EXPECT_FALSE(s.gpsSupported());
  EXPECT_FALSE(s.gpsEnabled());
  s.setGpsEnabled(true);             // default is a no-op
  EXPECT_FALSE(s.gpsEnabled());
  EXPECT_FALSE(s.gpsHasFix());
  EXPECT_EQ(0, s.gpsSatellites());
}

TEST(Applet, LifecycleHooksAreCallable) {
  RecordingApplet a;
  AppletContext ctx;
  a.onStart(ctx);
  a.onForeground();
  a.onBackground();
  a.onStop();
  EXPECT_EQ(1, a.started);
  EXPECT_EQ(1, a.foreground);
  EXPECT_EQ(1, a.background);
  EXPECT_EQ(1, a.stopped);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
