#include <gtest/gtest.h>
#include <mishmesh/core/InputMapping.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/InputSource.h>
#include "FakeDisplayDriver.h"
#include <deque>
#include <vector>

using namespace mishmesh;

namespace {
// Releases queued events on poll(), as a real source would when drained.
struct QueueSource : InputSource {
  std::deque<InputEvent> q;
  bool poll(InputReport& out) override {
    if (q.empty()) return false;
    out.event = q.front(); out.ch = 0; q.pop_front();
    return true;
  }
};
// Records every event the host actually delivers.
struct RecordingApplet : Applet {
  std::vector<InputEvent> got;
  RecordingApplet() : Applet("rec") {}
  int onRender(Canvas&) override { return 500; }
  bool onInput(InputEvent ev) override { got.push_back(ev); return true; }
};
}  // namespace

TEST(MapGesture, MapsEachGestureToConfiguredEvent) {
  GestureMap m;
  m.click = InputEvent::NavDown;
  m.doubleClick = InputEvent::Select;
  m.longPress = InputEvent::Back;
  // tripleClick left as default (None)

  EXPECT_EQ(InputEvent::NavDown, mapGesture(m, Gesture::Click));
  EXPECT_EQ(InputEvent::Select,  mapGesture(m, Gesture::DoubleClick));
  EXPECT_EQ(InputEvent::Back,    mapGesture(m, Gesture::LongPress));
  EXPECT_EQ(InputEvent::None,    mapGesture(m, Gesture::TripleClick));
  EXPECT_EQ(InputEvent::None,    mapGesture(m, Gesture::None));
}

TEST(MapDirection, DefaultMapMatchesNavDirections) {
  DirectionalMap m;  // defaults: up->NavUp, ..., press->Select
  EXPECT_EQ(InputEvent::NavUp,    mapDirection(m, Direction::Up));
  EXPECT_EQ(InputEvent::NavDown,  mapDirection(m, Direction::Down));
  EXPECT_EQ(InputEvent::NavLeft,  mapDirection(m, Direction::Left));
  EXPECT_EQ(InputEvent::NavRight, mapDirection(m, Direction::Right));
  EXPECT_EQ(InputEvent::Select,   mapDirection(m, Direction::Press));
}

TEST(MapDirection, RespectsCustomPressEvent) {
  DirectionalMap m;
  m.press = InputEvent::Back;
  EXPECT_EQ(InputEvent::Back, mapDirection(m, Direction::Press));
}

// Debounce belongs to the source, which is the only layer that sees real time.
// The host used to coalesce identical events within 60ms, which on a panel that
// blocks for half a second could not tell a bounce from a deliberate second tap.
TEST(InputDelivery, IdenticalEventsInOneDrainAreBothDelivered) {
  FakeDisplayDriver d;
  RecordingApplet app;
  AppletContext ctx;
  AppletHost host(&d, ctx);
  QueueSource src;
  host.addSource(&src);
  host.setRoot(&app);

  src.q = {InputEvent::NavDown, InputEvent::NavDown};
  host.loop(0);
  EXPECT_EQ(2u, app.got.size());
}


// Rotation belongs to the source, not to the event stream: a joystick is mounted
// in a direction, a button that emits NavDown is a labelled function. Doing it
// once in the host rotated both, because by then they look identical.
TEST(InputRotation, TurnsTheRingAndLeavesEverythingElseAlone) {
  EXPECT_EQ(InputEvent::NavLeft,  rotateDirection(InputEvent::NavUp, 1));
  EXPECT_EQ(InputEvent::NavUp,    rotateDirection(InputEvent::NavRight, 1));
  EXPECT_EQ(InputEvent::NavDown,  rotateDirection(InputEvent::NavUp, 2));
  EXPECT_EQ(InputEvent::NavRight, rotateDirection(InputEvent::NavUp, 3));
  EXPECT_EQ(InputEvent::NavUp,    rotateDirection(InputEvent::NavUp, 0));

  for (int q = 0; q < 4; q++) {   // no direction to turn
    EXPECT_EQ(InputEvent::Select, rotateDirection(InputEvent::Select, q));
    EXPECT_EQ(InputEvent::Back,   rotateDirection(InputEvent::Back, q));
    EXPECT_EQ(InputEvent::BackLong, rotateDirection(InputEvent::BackLong, q));
  }
}

TEST(InputRotation, AButtonThatEmitsADirectionIsNotTurned) {
  // QueueSource stands in for ButtonGestureSource: it takes the default
  // setRotation(), which ignores it. The Wio L1's user button emits NavDown as a
  // labelled shortcut, and turning the screen must not repoint it.
  FakeDisplayDriver d;
  RecordingApplet app;
  AppletContext ctx;
  AppletHost host(&d, ctx);
  QueueSource src;
  host.addSource(&src);
  host.setRoot(&app);
  host.setInputRotation(1);

  src.q = {InputEvent::NavDown};
  host.loop(10);
  ASSERT_EQ(1u, app.got.size());
  EXPECT_EQ(InputEvent::NavDown, app.got[0]);
}

namespace {
// Records what the host pushes down, standing in for a real source.
struct PolicySource : InputSource {
  uint16_t mask = 0xFFFF;   // a value no applet would return, so a push is visible
  bool poll(InputReport&) override { return false; }
  void setRepeatMask(uint16_t m) override { mask = m; }
};

struct NoRepeatApplet : Applet {
  NoRepeatApplet() : Applet("norepeat") {}
  int onRender(Canvas&) override { return 500; }
  bool onInput(InputEvent) override { return true; }
  uint16_t repeatMask() const override { return 0; }
};
}  // namespace

TEST(RepeatPolicy, DefaultScreenRepeatsTheVerticalAxisOnly) {
  RecordingApplet app;
  EXPECT_EQ(maskBit(InputEvent::NavUp) | maskBit(InputEvent::NavDown), app.repeatMask());
}

TEST(RepeatPolicy, ForegroundChangePushesTheScreensMaskToEverySource) {
  FakeDisplayDriver d;
  AppletContext ctx;
  AppletHost host(&d, ctx);
  PolicySource a, b;
  host.addSource(&a);
  host.addSource(&b);

  RecordingApplet root;
  host.setRoot(&root);
  EXPECT_EQ(maskBit(InputEvent::NavUp) | maskBit(InputEvent::NavDown), a.mask);
  EXPECT_EQ(maskBit(InputEvent::NavUp) | maskBit(InputEvent::NavDown), b.mask);

  NoRepeatApplet quiet;
  host.push(&quiet);
  EXPECT_EQ(0, a.mask);          // the screen on top decides
  EXPECT_EQ(0, b.mask);

  host.pop();
  EXPECT_EQ(maskBit(InputEvent::NavUp) | maskBit(InputEvent::NavDown), a.mask);
}

// UITask adds its sources after setRoot (not before, like every test above), so
// addSource must seed a late-joining source from the current foreground rather
// than leaving it at its constructed default.
TEST(RepeatPolicy, SourceAddedAfterSetRootIsSeededFromTheForeground) {
  FakeDisplayDriver d;
  AppletContext ctx;
  AppletHost host(&d, ctx);

  RecordingApplet root;
  host.setRoot(&root);

  PolicySource late;
  host.addSource(&late);
  EXPECT_EQ(maskBit(InputEvent::NavUp) | maskBit(InputEvent::NavDown), late.mask);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
