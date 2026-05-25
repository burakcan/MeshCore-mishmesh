#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/applets/AppMenuApplet.h>
#include <mishmesh/applets/StopwatchApplet.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {

class LeafApplet : public Applet {
public:
  explicit LeafApplet(const char* n) : Applet(n) {}
  int onRender(Canvas&) override { return 1000; }
};

LeafApplet alpha("alpha"), beta("beta");

// Registry is a shared global; each test rebuilds it deterministically.
void registerTwo() {
  resetRegistry();
  static AppletRegistration rb{&beta, "Beta", 0, Placement::AppMenu, 20, nullptr};
  static AppletRegistration ra{&alpha, "Alpha", 0, Placement::AppMenu, 10, nullptr};
  static AppletRegistration rhidden{&beta, "Hidden", 0, Placement::LaunchOnly, 0, nullptr};
  registerApplet(&rb);
  registerApplet(&ra);
  registerApplet(&rhidden);
}

}  // namespace

TEST(AppMenu, ListsAppMenuRegistrationsSortedByOrder) {
  registerTwo();
  AppMenuApplet menu;
  AppletContext ctx;
  menu.onStart(ctx);
  ASSERT_EQ(2, menu.count());            // LaunchOnly excluded
  EXPECT_STREQ("Alpha", menu.label(0));  // order 10 before 20
  EXPECT_STREQ("Beta", menu.label(1));
}

TEST(AppMenu, SelectPushesHighlightedApplet) {
  registerTwo();
  FakeDisplayDriver d;
  AppletContext ctx;
  AppletHost host(&d, ctx);
  AppMenuApplet menu;
  host.setRoot(&menu);

  host.dispatch(InputEvent::Select);     // launch first entry
  EXPECT_EQ(2, host.depth());
  EXPECT_EQ(&alpha, host.foreground());
}

TEST(AppMenu, NavThenSelectPushesSecondApplet) {
  registerTwo();
  FakeDisplayDriver d;
  AppletContext ctx;
  AppletHost host(&d, ctx);
  AppMenuApplet menu;
  host.setRoot(&menu);

  host.dispatch(InputEvent::NavDown);
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(&beta, host.foreground());
}

TEST(AppMenu, BackBubblesToHostAndIsNotConsumed) {
  registerTwo();
  FakeDisplayDriver d;
  AppletContext ctx;
  AppletHost host(&d, ctx);
  AppMenuApplet menu;
  host.setRoot(&menu);
  host.push(&alpha);
  EXPECT_EQ(2, host.depth());
  host.dispatch(InputEvent::Back);       // alpha doesn't consume -> pops
  EXPECT_EQ(1, host.depth());
}

// ---- Stopwatch -------------------------------------------------------------

TEST(Stopwatch, AccumulatesWhileRunning) {
  StopwatchApplet sw;
  sw.toggle(1000);                       // start
  EXPECT_EQ(0u, sw.elapsedMs(1000));
  EXPECT_EQ(1500u, sw.elapsedMs(2500));
}

TEST(Stopwatch, FreezesWhenStopped) {
  StopwatchApplet sw;
  sw.toggle(1000);
  sw.toggle(2500);                       // stop at 1500ms
  EXPECT_EQ(1500u, sw.elapsedMs(9999));
}

TEST(Stopwatch, ResumesFromBankedTime) {
  StopwatchApplet sw;
  sw.toggle(1000);
  sw.toggle(2500);                       // banked 1500
  sw.toggle(3000);                       // resume
  EXPECT_EQ(2000u, sw.elapsedMs(3500));
}

TEST(Stopwatch, ResetClearsElapsed) {
  StopwatchApplet sw;
  sw.toggle(1000);
  sw.reset();
  EXPECT_EQ(0u, sw.elapsedMs(5000));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
