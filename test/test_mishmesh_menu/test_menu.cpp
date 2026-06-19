#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/applets/AppMenuApplet.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
// Vertical extent (px) touched by fillRect calls - for the empty-state, the only
// draws are the message glyphs, so this is the message's rendered height.
int fillsVSpan(const FakeDisplayDriver& d) {
  int lo = 1 << 30, hi = -(1 << 30);
  for (const auto& f : d.fills) { if (f.y < lo) lo = f.y; if (f.y + f.h > hi) hi = f.y + f.h; }
  return hi > lo ? hi - lo : 0;
}
struct EmptyModel : ListModel {
  int count() const override { return 0; }
  const char* label(int) const override { return ""; }
};
}

// A '\n' in the empty-state message must render as stacked lines, not a single
// line with a tofu block for the newline. Regression: the picker's
// "No quick replies\nAdd in Settings" showed a block + a clipped second line.
TEST(ListMenuEmptyText, NewlineRendersAsMultipleLines) {
  EmptyModel m;
  FakeDisplayDriver d1; Canvas c1(&d1);
  ListMenu a; a.setModel(&m); a.setEmptyText("AAA");
  a.draw(c1, 0, 0, 128, 64);

  FakeDisplayDriver d2; Canvas c2(&d2);
  ListMenu b; b.setModel(&m); b.setEmptyText("AAA\nBBB");
  b.draw(c2, 0, 0, 128, 64);

  int oneLine = fillsVSpan(d1);
  int twoLine = fillsVSpan(d2);
  EXPECT_GT(oneLine, 0);                 // single line renders something
  EXPECT_GT(twoLine, oneLine + 3);       // two lines are meaningfully taller (buggy: ~equal)
}

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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
