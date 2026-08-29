#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/core/Canvas.h>
#include "FakeDisplayDriver.h"

#include <vector>

using namespace mishmesh;

namespace {

class FakeApplet : public Applet {
public:
  int started = 0, foreground = 0, background = 0, stopped = 0, rendered = 0;
  int renderDelay = 1000;
  bool consume = false;            // if true, onInput consumes everything
  InputEvent lastInput = InputEvent::None;
  int inputs = 0;                  // dispatches that reached the applet

  bool overlay = false;            // if true, host composites the applet beneath

  bool keepWake = false;           // keepOnWake(): stay put on a long-sleep wake
  bool blockSleep = false;         // blocksSleep(): suppress auto-off
  int slept = 0;                   // onSleep() call count

  explicit FakeApplet(const char* n) : Applet(n) {}
  bool isOverlay() const override { return overlay; }
  bool keepOnWake() const override { return keepWake; }
  bool blocksSleep() const override { return blockSleep; }
  void onSleep() override { slept++; }
  void onStart(AppletContext&) override { started++; }
  void onForeground() override { foreground++; }
  void onBackground() override { background++; }
  void onStop() override { stopped++; }
  int onRender(Canvas&) override { rendered++; return renderDelay; }
  bool onInput(InputEvent ev) override { lastInput = ev; inputs++; return consume; }
};

class QueueSource : public InputSource {
public:
  std::vector<InputEvent> queue;
  size_t idx = 0;
  bool poll(InputReport& out) override {
    if (idx >= queue.size()) return false;
    out.event = queue[idx++];
    out.ch = 0;
    return true;
  }
};

AppletContext emptyCtx() { return AppletContext{}; }

}  // namespace

TEST(AppletHost, SetRootStartsAndForegroundsIt) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  EXPECT_EQ(1, host.depth());
  EXPECT_EQ(&root, host.foreground());
  EXPECT_EQ(1, root.started);
  EXPECT_EQ(1, root.foreground);
}

TEST(AppletHost, PushBackgroundsPreviousAndForegroundsNew) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root"), child("child");
  host.setRoot(&root);
  host.push(&child);
  EXPECT_EQ(2, host.depth());
  EXPECT_EQ(&child, host.foreground());
  EXPECT_EQ(1, root.background);
  EXPECT_EQ(1, child.started);
  EXPECT_EQ(1, child.foreground);
}

TEST(AppletHost, PopStopsTopAndForegroundsRevealed) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root"), child("child");
  host.setRoot(&root);
  host.push(&child);
  host.pop();
  EXPECT_EQ(1, host.depth());
  EXPECT_EQ(&root, host.foreground());
  EXPECT_EQ(1, child.stopped);
  EXPECT_EQ(2, root.foreground);  // initial + revealed
}

TEST(AppletHost, ReplaceSwapsTopKeepingDepthAndUnderlyingApplet) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root"), first("first"), second("second");
  host.setRoot(&root);
  host.push(&first);
  host.replace(&second);
  EXPECT_EQ(2, host.depth());            // depth unchanged
  EXPECT_EQ(&second, host.foreground());
  EXPECT_EQ(1, first.stopped);           // replaced applet was stopped
  EXPECT_EQ(1, second.started);
  EXPECT_EQ(1, second.foreground);
  // Back from the replacement returns to what was underneath, not to `first`.
  host.pop();
  EXPECT_EQ(&root, host.foreground());
  EXPECT_EQ(1, second.stopped);
}

TEST(AppletHost, PopAtRootIsNoOp) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  host.pop();
  EXPECT_EQ(1, host.depth());
  EXPECT_EQ(0, root.stopped);
}

// Idle the host past auto-off so the panel sleeps, having first registered one
// input event at `activityAt` (auto-off is input-gated). Returns the now_ms the
// sleep loop ran at, so callers can wake relative to it.
static uint32_t sleepPanel(AppletHost& host, QueueSource& src,
                           FakeDisplayDriver& d, uint32_t activityAt) {
  src.queue.push_back(InputEvent::NavDown);
  host.loop(activityAt);                 // input consumed -> last_activity = activityAt
  uint32_t sleepAt = activityAt + 30001; // just past the 30s auto-off
  host.loop(sleepAt);
  EXPECT_FALSE(d.isOn());
  return sleepAt;
}

TEST(AppletHost, LongSleepWakeResetsToHome) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  QueueSource src; host.addSource(&src);
  host.setAutoOffMillis(30000);
  FakeApplet root("root"), child("child");
  host.setRoot(&root);
  host.push(&child);

  uint32_t sleptAt = sleepPanel(host, src, d, 1000);
  EXPECT_EQ(1, child.slept);             // onSleep fired on the foreground

  src.queue.push_back(InputEvent::Select);
  host.loop(sleptAt + 60001);            // wake past the 60s home threshold
  EXPECT_TRUE(d.isOn());
  EXPECT_EQ(1, host.depth());            // reset to home
  EXPECT_EQ(&root, host.foreground());
  EXPECT_EQ(InputEvent::None, root.lastInput);   // wake press only woke, not delivered
}

TEST(AppletHost, LongSleepWakeKeepsAppletThatOptsIn) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  QueueSource src; host.addSource(&src);
  host.setAutoOffMillis(30000);
  FakeApplet root("root"), child("child");
  child.keepWake = true;                 // e.g. an open chat / running timer
  host.setRoot(&root);
  host.push(&child);

  uint32_t sleptAt = sleepPanel(host, src, d, 1000);
  src.queue.push_back(InputEvent::Select);
  host.loop(sleptAt + 60001);
  EXPECT_EQ(2, host.depth());            // stayed put
  EXPECT_EQ(&child, host.foreground());
}

TEST(AppletHost, ShortNapWakeKeepsPlace) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  QueueSource src; host.addSource(&src);
  host.setAutoOffMillis(30000);
  FakeApplet root("root"), child("child");
  host.setRoot(&root);
  host.push(&child);

  uint32_t sleptAt = sleepPanel(host, src, d, 1000);
  src.queue.push_back(InputEvent::Select);
  host.loop(sleptAt + 5000);             // woke well under the 60s threshold
  EXPECT_EQ(2, host.depth());            // kept place despite no keepOnWake
  EXPECT_EQ(&child, host.foreground());
}

TEST(AppletHost, BlocksSleepHoldsPanelOnThenGraceOnRelease) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  QueueSource src; host.addSource(&src);
  host.setAutoOffMillis(30000);
  FakeApplet root("root"), child("child");
  child.blockSleep = true;               // e.g. a running stopwatch
  host.setRoot(&root);
  host.push(&child);

  src.queue.push_back(InputEvent::NavDown);
  host.loop(1000);
  host.loop(1000 + 30001);               // past auto-off, but sleep is blocked
  host.loop(1000 + 90001);
  EXPECT_TRUE(d.isOn());
  EXPECT_EQ(0, child.slept);

  child.blockSleep = false;              // stopwatch stopped
  host.loop(1000 + 90001 + 15000);       // within the fresh grace window
  EXPECT_TRUE(d.isOn());
  host.loop(1000 + 90001 + 30002);       // full auto-off after release
  EXPECT_FALSE(d.isOn());
}

TEST(AppletHost, UnconsumedBackPopsTheStack) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root"), child("child");
  child.consume = false;
  host.setRoot(&root);
  host.push(&child);
  host.dispatch(InputEvent::Back);
  EXPECT_EQ(1, host.depth());          // popped back to root
  EXPECT_EQ(InputEvent::Back, child.lastInput);
}

TEST(AppletHost, ConsumedEventDoesNotBubble) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root"), child("child");
  child.consume = true;
  host.setRoot(&root);
  host.push(&child);
  host.dispatch(InputEvent::Back);
  EXPECT_EQ(2, host.depth());          // stayed; child ate the Back
}

TEST(AppletHost, LoopDrainsSourcesAndRoutesToForeground) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  root.consume = true;
  host.setRoot(&root);
  QueueSource src;
  src.queue.push_back(InputEvent::Select);
  host.addSource(&src);
  host.loop(0);
  EXPECT_EQ(InputEvent::Select, root.lastInput);
}

TEST(AppletHost, RendersOnceThenRespectsDelay) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  root.renderDelay = 1000;
  host.setRoot(&root);
  host.loop(0);
  EXPECT_EQ(1, root.rendered);
  host.loop(500);    // before the 1000ms delay elapses
  EXPECT_EQ(1, root.rendered);
  host.loop(1000);   // delay elapsed
  EXPECT_EQ(2, root.rendered);
}

TEST(AppletHost, InputForcesImmediateRedraw) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  root.renderDelay = 100000;  // would not naturally redraw soon
  root.consume = true;
  host.setRoot(&root);
  host.loop(0);
  EXPECT_EQ(1, root.rendered);
  QueueSource src;
  src.queue.push_back(InputEvent::NavDown);
  host.addSource(&src);
  host.loop(10);     // input arrives -> forced redraw despite the long delay
  EXPECT_EQ(2, root.rendered);
}

TEST(AppletHost, OverlayCompositesUnderlayEachFrame) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root"), banner("banner");
  banner.overlay = true;
  host.setRoot(&root);
  host.loop(0);
  EXPECT_EQ(1, root.rendered);
  host.push(&banner);
  host.loop(10);           // push -> dirty -> both drawn, underlay first
  EXPECT_EQ(2, root.rendered);
  EXPECT_EQ(1, banner.rendered);
}

TEST(AppletHost, NonOverlayChildDoesNotRenderUnderlay) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root"), child("child");
  host.setRoot(&root);
  host.loop(0);
  host.push(&child);
  host.loop(10);
  EXPECT_EQ(1, root.rendered);
  EXPECT_EQ(1, child.rendered);
}

TEST(AppletHost, OverlayMergesUnderlayDelay) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root"), banner("banner");
  banner.overlay = true;
  root.renderDelay = 100;     // underlay animates faster than the overlay
  banner.renderDelay = 100000;
  host.setRoot(&root);
  host.push(&banner);
  host.loop(0);
  EXPECT_EQ(1, root.rendered);
  host.loop(100);             // due at the underlay's shorter delay
  EXPECT_EQ(2, root.rendered);
  EXPECT_EQ(2, banner.rendered);
}

TEST(AppletHost, RenderWrapsInStartAndEndFrame) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  host.loop(0);
  ASSERT_GE(d.calls.size(), 2u);
  EXPECT_EQ("startFrame", d.calls.front());
  EXPECT_EQ("endFrame", d.calls.back());
}

TEST(AppletHost, SetRootTurnsDisplayOn) {
  FakeDisplayDriver d;
  d.turnOff();              // panels boot off; only turnOn() lights them
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  EXPECT_TRUE(d.isOn());
}

TEST(AppletHost, TurnsDisplayOffAfterInactivity) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setAutoOffMillis(1000);
  host.setRoot(&root);
  host.loop(0);            // first activity timestamp
  EXPECT_TRUE(d.isOn());
  host.loop(1001);
  EXPECT_FALSE(d.isOn());
}

TEST(AppletHost, DoesNotRenderWhileDisplayOff) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  root.renderDelay = 10;
  host.setAutoOffMillis(1000);
  host.setRoot(&root);
  host.loop(0);
  host.loop(1001);         // sleeps
  int rendered = root.rendered;
  host.loop(2000);         // would be render-due, but display is off
  EXPECT_EQ(rendered, root.rendered);
}

TEST(AppletHost, InputWakesDisplayAndIsSwallowed) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setAutoOffMillis(1000);
  host.setRoot(&root);
  host.loop(0);
  host.loop(1001);         // sleeps
  ASSERT_FALSE(d.isOn());

  QueueSource src;
  src.queue.push_back(InputEvent::Select);
  host.addSource(&src);
  host.loop(1002);
  EXPECT_TRUE(d.isOn());                          // woke
  EXPECT_EQ(InputEvent::None, root.lastInput);    // wake event not delivered
}

TEST(AppletHost, InputWhileOnIsDeliveredAndExtendsTimer) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setAutoOffMillis(1000);
  host.setRoot(&root);
  host.loop(0);

  QueueSource src;
  src.queue.push_back(InputEvent::NavDown);
  host.addSource(&src);
  host.loop(900);          // input delivered, deadline now extends from 900
  EXPECT_EQ(InputEvent::NavDown, root.lastInput);
  host.loop(1500);
  EXPECT_TRUE(d.isOn());    // 1500-900 < 1000
  host.loop(2000);
  EXPECT_FALSE(d.isOn());   // 2000-900 > 1000
}

// Input is polled both before and after the (synchronous, possibly slow) render so a
// heavy frame can't open an input-blind gap. This source stays quiet on the first
// poll of a loop() and only reports on the second - emulating a press that lands while
// the frame is being composed. It must still be delivered within the same loop(),
// which only happens if the host polls again after rendering.
class ExclusiveApplet : public Applet {
public:
  int rendered = 0;
  explicit ExclusiveApplet(const char* n) : Applet(n) {}
  bool wantsExclusive() const override { return true; }
  int onRender(Canvas&) override { rendered++; return 10000; }   // big delay; must be ignored
};

class HeldSource : public InputSource {
public:
  uint16_t mask = 0;
  bool poll(InputReport&) override { return false; }   // no discrete events
  uint16_t heldMask() const override { return mask; }
};

class SecondPollSource : public InputSource {
public:
  int polls = 0;
  bool fired = false;
  InputEvent ev;
  explicit SecondPollSource(InputEvent e) : ev(e) {}
  bool poll(InputReport& out) override {
    polls++;
    if (polls == 2 && !fired) { fired = true; out.event = ev; out.ch = 0; return true; }
    return false;
  }
};

TEST(AppletHost, InputIsPolledAgainAfterRender) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);

  SecondPollSource src(InputEvent::Select);
  host.addSource(&src);
  host.loop(10);
  // Delivered in the post-render poll of this same loop(), not deferred to the next.
  EXPECT_EQ(InputEvent::Select, root.lastInput);
  EXPECT_GE(src.polls, 2);
}

// A repeated tap where the first press is caught by the pre-render poll and the
// second by the post-render one. Both carry the same pre-render now_ms, so on a
// panel whose flush takes hundreds of ms the 60ms coalescing used to swallow the
// second press - what made keypad multi-tap and list nav drop presses on e-ink.
class TapTwiceSource : public InputSource {
public:
  int polls = 0;
  InputEvent ev;
  explicit TapTwiceSource(InputEvent e) : ev(e) {}
  bool poll(InputReport& out) override {
    polls++;
    if (polls == 1 || polls == 3) { out.event = ev; return true; }
    return false;
  }
};

TEST(AppletHost, RepeatedTapSurvivesASlowFlush) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  TapTwiceSource src(InputEvent::Select);
  host.addSource(&src);

  host.loop(10);

  EXPECT_EQ(2, root.inputs);
}

// A repaint nothing asked for is held to the minimum flush spacing; one the user
// triggered is not. Getting this backwards is what made the panel refresh back to
// back and swallow presses.
TEST(AppletHost, ReducedMotionRateLimitsTimerRepaints) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  root.renderDelay = 250;                  // a typical idle re-poll
  host.setRoot(&root);

  setReducedMotion(true);
  host.loop(0);
  const int base = root.rendered;
  host.loop(300);                          // applet says due, spacing says wait
  const int held = root.rendered - base;
  host.loop(1200);                         // spacing elapsed
  const int released = root.rendered - base;
  setReducedMotion(false);

  EXPECT_EQ(0, held);
  EXPECT_EQ(1, released);   // deferred, not dropped
}

TEST(AppletHost, ReducedMotionNeverDelaysInputRepaints) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  root.renderDelay = 250;
  host.setRoot(&root);
  QueueSource src;
  host.addSource(&src);

  setReducedMotion(true);
  host.loop(0);
  const int base = root.rendered;
  src.queue.push_back(InputEvent::NavDown);
  host.loop(50);                           // well inside the spacing window
  const int after = root.rendered - base;
  setReducedMotion(false);

  EXPECT_EQ(1, after);   // a press repaints now, spacing or not
}

// A bistable panel keeps its last flush, so going to sleep has to leave a blank
// frame behind - otherwise the dead UI stays on the glass and the next press,
// which wakes to home, looks like the screen was lost.
TEST(AppletHost, SleepBlanksABistablePanel) {
  FakeDisplayDriver d;
  d.eink = true;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  host.setAutoOffMillis(1000);

  host.loop(0);
  d.calls.clear();
  host.loop(5000);                     // idle past the auto-off deadline

  int frames = 0;
  for (const auto& c : d.calls) if (c == "endFrame") frames++;
  EXPECT_FALSE(d.on);                  // asleep
  EXPECT_GE(frames, 1);                // and something was flushed on the way out
}

TEST(AppletHost, SleepDoesNotRepaintAPanelThatBlanksItself) {
  FakeDisplayDriver d;                 // eink defaults false
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  host.setAutoOffMillis(1000);

  host.loop(0);
  d.calls.clear();
  host.loop(5000);

  int frames = 0;
  for (const auto& c : d.calls) if (c == "endFrame") frames++;
  EXPECT_FALSE(d.on);
  EXPECT_EQ(0, frames);                // turnOff() is enough on an OLED
}

TEST(InputState, IsDownReflectsHeldBits) {
  InputState s;
  EXPECT_FALSE(s.isDown(InputEvent::NavLeft));
  s.held = maskBit(InputEvent::NavLeft) | maskBit(InputEvent::NavRight);
  EXPECT_TRUE(s.isDown(InputEvent::NavLeft));
  EXPECT_TRUE(s.isDown(InputEvent::NavRight));
  EXPECT_FALSE(s.isDown(InputEvent::NavUp));
  EXPECT_FALSE(s.isDown(InputEvent::Select));
}

TEST(AppletHost, AggregatesHeldMaskFromSources) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);

  HeldSource a, b;
  host.addSource(&a);
  host.addSource(&b);
  a.mask = maskBit(InputEvent::NavLeft);
  b.mask = maskBit(InputEvent::Select);

  host.loop(0);
  const InputState& in = host.ctxInput();   // test accessor, see Step 4
  EXPECT_TRUE(in.isDown(InputEvent::NavLeft));
  EXPECT_TRUE(in.isDown(InputEvent::Select));
  EXPECT_FALSE(in.isDown(InputEvent::NavRight));

  a.mask = 0; b.mask = 0;
  host.loop(10);
  EXPECT_FALSE(host.ctxInput().isDown(InputEvent::NavLeft));
}

TEST(AppletHost, ExclusiveAppletRendersEveryPassIgnoringDelay) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  ExclusiveApplet game("game");
  host.setRoot(&game);

  // Despite returning a 10s delay, it must render on each loop pass.
  host.loop(0);
  host.loop(1);
  host.loop(2);
  EXPECT_EQ(3, game.rendered);
}

TEST(AppletHost, NonExclusiveAppletStillRespectsDelay) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  root.renderDelay = 1000;
  host.setRoot(&root);

  host.loop(0);          // first render (forced)
  int after_first = root.rendered;
  host.loop(1);          // within the 1000ms delay window: must NOT re-render
  EXPECT_EQ(after_first, root.rendered);
}

TEST(AppletHost, ExclusiveFlushIsRateCappedButRenderIsNot) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  ExclusiveApplet game("game");
  host.setRoot(&game);

  // Drive 0..48ms at 1ms steps: 49 passes.
  for (uint32_t t = 0; t <= 48; t++) host.loop(t);

  // onRender (and startFrame) every pass.
  EXPECT_EQ(49, game.rendered);
  int startFrames = 0;
  for (const auto& c : d.calls) if (c == "startFrame") startFrames++;
  EXPECT_EQ(49, startFrames);

  // endFrame capped at 60fps (>=16ms apart): flushes at t=0,16,32,48 -> 4.
  int endFrames = 0;
  for (const auto& c : d.calls) if (c == "endFrame") endFrames++;
  EXPECT_EQ(4, endFrames);
}

TEST(AppletHost, LastInputMsTracksTheMostRecentDispatch) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  QueueSource src;
  host.addSource(&src);

  host.loop(1000);
  EXPECT_EQ(0u, host.lastInputMs());     // nothing pressed yet

  src.queue.push_back(InputEvent::NavDown);
  host.loop(2500);
  EXPECT_EQ(2500u, host.lastInputMs());

  host.loop(4000);                        // no new input: stamp stays put
  EXPECT_EQ(2500u, host.lastInputMs());
}

// The bug this guards: _last_activity also advances on every loop pass while a
// blocksSleep() applet holds the screen awake (e.g. a running stopwatch), which
// is not input. lastInputMs() must not follow that - or the unread reminder's
// "keep nagging until a key is pressed" is defeated by the screen simply staying
// on, with nobody having looked at it.
TEST(AppletHost, LastInputMsIgnoresTheBlocksSleepKeepAlive) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  host.setAutoOffMillis(30000);
  FakeApplet root("root");
  root.blockSleep = true;               // e.g. a running stopwatch
  host.setRoot(&root);

  host.loop(1000);
  EXPECT_EQ(0u, host.lastInputMs());

  host.loop(31000);
  host.loop(61000);
  host.loop(91000);
  EXPECT_EQ(0u, host.lastInputMs());     // still no real input, despite the keep-alive
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
