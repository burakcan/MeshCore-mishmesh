#include <gtest/gtest.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

TEST(Keypad, LowerModeLabels) {
  KeypadApplet k;
  EXPECT_STREQ(".,?!", k.cellLabel(0, 0));
  EXPECT_STREQ("abc",  k.cellLabel(0, 1));
  EXPECT_STREQ("def",  k.cellLabel(0, 2));
  EXPECT_STREQ("wxyz", k.cellLabel(2, 2));
  // action cells
  EXPECT_STREQ("DEL",  k.cellLabel(0, 3));
  EXPECT_STREQ("SPC",  k.cellLabel(1, 3));
  EXPECT_STREQ("ab",   k.cellLabel(2, 3));   // shift cell shows current mode
  EXPECT_STREQ("sym",  k.cellLabel(3, 0));
  EXPECT_STREQ("OK",   k.cellLabel(3, 3));
}

TEST(Keypad, CycleModeChangesLabels) {
  KeypadApplet k;
  EXPECT_EQ(KeypadApplet::Mode::Lower, k.mode());
  k.cycleMode();
  EXPECT_EQ(KeypadApplet::Mode::Upper, k.mode());
  EXPECT_STREQ("ABC", k.cellLabel(0, 1));
  EXPECT_STREQ("AB",  k.cellLabel(2, 3));
  k.cycleMode();
  EXPECT_EQ(KeypadApplet::Mode::Num, k.mode());
  EXPECT_STREQ("2", k.cellLabel(0, 1));      // abc cell -> digit 2
  EXPECT_STREQ("0", k.cellLabel(3, 0));      // sym cell -> digit 0 in Num mode
  EXPECT_STREQ("12", k.cellLabel(2, 3));
  k.cycleMode();
  EXPECT_EQ(KeypadApplet::Mode::Lower, k.mode());
}

TEST(Keypad, SymPageSwapsCharCells) {
  KeypadApplet k;
  EXPECT_FALSE(k.symPage());
  k.toggleSymPage();
  EXPECT_TRUE(k.symPage());
  EXPECT_STREQ(":;\"'", k.cellLabel(0, 1));
  EXPECT_STREQ("abc",   k.cellLabel(3, 0));  // sym cell becomes return-to-letters
  k.toggleSymPage();
  EXPECT_FALSE(k.symPage());
  EXPECT_STREQ("abc", k.cellLabel(0, 1));
}

TEST(Keypad, CellIconsWiredForActionCells) {
  KeypadApplet k;
  EXPECT_EQ(0, k.cellIcon(0, 1));                          // char cell: text
  EXPECT_EQ(0, k.cellIcon(2, 3));                          // shift cell: text
  EXPECT_EQ((uint16_t)Icon::Backspace,  k.cellIcon(0, 3)); // DEL
  EXPECT_EQ((uint16_t)Icon::ArrowLeft,  k.cellIcon(3, 1)); // cursor left
  EXPECT_EQ((uint16_t)Icon::ArrowRight, k.cellIcon(3, 2)); // cursor right
  EXPECT_EQ((uint16_t)Icon::Check,      k.cellIcon(3, 3)); // OK
}

namespace {
// Start an applet (runs onStart) and return the host so tests can drive input.
struct Harness {
  FakeDisplayDriver d;
  AppletContext ctx;
  AppletHost host;
  Harness(KeypadApplet* k) : host(&d, ctx) { host.setRoot(k); }
  void tick(KeypadApplet* k, uint32_t now) { Canvas c(&d, now); k->onRender(c); }
};
}

TEST(Keypad, TypeSingleTapInsertsFirstLetter) {
  KeypadApplet k; Harness h(&k);
  k.onInput(InputEvent::Select);     // 'a' pending (opens focused on "abc")
  EXPECT_STREQ("a", k.text());
  EXPECT_EQ(1, k.cursor());
}

TEST(Keypad, MultiTapCyclesWithinGroup) {
  KeypadApplet k; Harness h(&k);                  // opens focused on "abc"
  k.onInput(InputEvent::Select);                  // a
  k.onInput(InputEvent::Select);                  // b
  k.onInput(InputEvent::Select);                  // c
  EXPECT_STREQ("c", k.text());
  EXPECT_EQ(1, k.cursor());                        // still one char, cycling in place
  k.onInput(InputEvent::Select);                  // wraps c -> a
  EXPECT_STREQ("a", k.text());
}

TEST(Keypad, MultiTapSurvivesStaleRenderClock) {
  // Regression: the multi-tap timer must run off a fresh clock. onInput() only sees
  // the _now cached by the previous render, which after an idle frame can be ~1s
  // stale. A tap therefore defers its timestamp to the next render. Before the fix,
  // the first post-tap render saw a huge bogus elapsed and committed instantly, so a
  // quick second tap typed "aa" instead of cycling to "b".
  KeypadApplet k; Harness h(&k);     // opens focused on "abc"
  h.tick(&k, 0);                     // last render at t=0 (idle)
  k.onInput(InputEvent::Select);     // tap 'a' while the cached render clock is stale (0)
  h.tick(&k, 1000);                  // clock jumps 1s (idle gap) - must NOT commit the pending tap
  k.onInput(InputEvent::Select);     // quick second tap, same cell -> must cycle to 'b'
  EXPECT_STREQ("b", k.text());
  EXPECT_EQ(1, k.cursor());
}

TEST(Keypad, TimeoutCommitsThenNewCharSameGroup) {
  KeypadApplet k; Harness h(&k);                  // opens focused on "abc"
  h.tick(&k, 100);
  k.onInput(InputEvent::Select);                  // 'a' pending at t=100 (uses cached _now)
  h.tick(&k, 100);                                // pending, not timed out
  h.tick(&k, 1000);                               // 900ms later >= 800 -> commit
  k.onInput(InputEvent::Select);                  // new char 'a' (not a cycle to 'b')
  EXPECT_STREQ("aa", k.text());
  EXPECT_EQ(2, k.cursor());
}

TEST(Keypad, MovingFocusCommitsPending) {
  KeypadApplet k; Harness h(&k);                  // opens focused on "abc"
  k.onInput(InputEvent::Select); k.onInput(InputEvent::Select); // 'b' pending
  k.onInput(InputEvent::NavRight);                // focus "def", commits 'b'
  k.onInput(InputEvent::Select);                  // 'd'
  EXPECT_STREQ("bd", k.text());
}

TEST(Keypad, BackspaceDeletesBeforeCursor) {
  KeypadApplet k; Harness h(&k);                  // opens focused on "abc"
  k.onInput(InputEvent::Select);                  // 'a'
  k.onInput(InputEvent::NavRight);                // commit, focus def
  k.onInput(InputEvent::Select);                  // 'd' -> "ad"
  EXPECT_TRUE(k.onInput(InputEvent::Back));       // backspace
  EXPECT_STREQ("a", k.text());
  EXPECT_EQ(1, k.cursor());
}

TEST(Keypad, BackOnEmptyBubblesToPop) {
  KeypadApplet k; Harness h(&k);
  EXPECT_FALSE(k.onInput(InputEvent::Back));      // empty -> host pops
}

TEST(Keypad, SpaceCellAndCursorMove) {
  KeypadApplet k; Harness h(&k);                  // opens focused on "abc" = (0,1)
  k.onInput(InputEvent::Select);                  // 'a'
  // focus SPC cell (1,3): from (0,1) -> NavDown to (1,1), NavRight x2 to (1,3)
  k.onInput(InputEvent::NavDown);
  k.onInput(InputEvent::NavRight); k.onInput(InputEvent::NavRight);
  k.onInput(InputEvent::Select);                  // space -> "a "
  EXPECT_STREQ("a ", k.text());
  EXPECT_EQ(2, k.cursor());
  // cursor left cell (3,1): navigate there and move cursor left
  k.setFocusForTest(3, 1);                        // helper added below
  k.onInput(InputEvent::Select);
  EXPECT_EQ(1, k.cursor());
}

TEST(Keypad, NumModeTapsInsertDigits) {
  KeypadApplet k; Harness h(&k);
  k.cycleMode(); k.cycleMode();                   // -> Num (focus stays on (0,1) = "2")
  k.onInput(InputEvent::Select);
  k.onInput(InputEvent::Select);                  // single-char group: two '2's
  EXPECT_STREQ("22", k.text());
}

TEST(Keypad, RenderDrawsCursorFill) {
  KeypadApplet k; Harness h(&k);                  // opens focused on "abc"
  k.onInput(InputEvent::Select);                  // 'a'
  // Canvas(&d, 10): 10 is frame-time (now), NOT width - width comes from d->width()=128.
  // drawBuffer gets w=128-20=108, so the 1px cursor bar fillRect fires normally.
  Canvas c(&h.d, 10);
  k.onRender(c);
  // Grid highlight fills (cw=32) would satisfy size()>0 alone; assert the cursor bar
  // specifically: drawBuffer always emits a w==1 fillRect at the cursor x position.
  bool hasCursorBar = false;
  for (auto& f : h.d.fills) if (f.w == 1) { hasCursorBar = true; break; }
  EXPECT_TRUE(hasCursorBar) << "cursor bar (w==1 fill) not rendered by drawBuffer";
}

TEST(Keypad, ConfirmTogglesPopAndToast) {
  KeypadApplet k;
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  // root applet must exist beneath so pop() has somewhere to land:
  static KeypadApplet root;            // any applet works as a stand-in root
  host.setRoot(&root);
  host.push(&k);
  EXPECT_EQ(2, host.depth());
  k.onInput(InputEvent::Select);       // type 'a' (opens focused on "abc")
  k.setFocusForTest(3, 3);             // OK cell
  k.onInput(InputEvent::Select);       // confirm
  EXPECT_EQ(1, host.depth());          // popped back to root
  // TODO: assert toast content ("Saved") - AppletHost has no public toast accessor
  //       (_toast_msg is private; postToast() is write-only from the outside).
}

TEST(Keypad, RegisteredInAppMenu) {
  bool found = false;
  for (AppletRegistration* r = registeredApplets(); r; r = r->next) {
    if (r->placement == Placement::AppMenu && strcmp(r->label, "Keypad") == 0) found = true;
  }
  EXPECT_TRUE(found);
}

namespace {
// Records the host's setHoldRepeat() calls so we can assert the per-applet
// Back-repeat preference is propagated on foreground changes.
struct RecordingSource : InputSource {
  bool holdRepeat = false;
  bool poll(InputReport&) override { return false; }
  void setHoldRepeat(bool e) override { holdRepeat = e; }
};
// A plain screen that does NOT want Back to repeat (the default).
struct PlainApplet : Applet {
  PlainApplet() : Applet("Plain") {}
  int onRender(Canvas&) override { return 1000; }
};
// Handles Select but not SelectLong (like the app menu / list screens).
struct SelectOnlyApplet : Applet {
  int selects = 0;
  SelectOnlyApplet() : Applet("SelOnly") {}
  int onRender(Canvas&) override { return 1000; }
  bool onInput(InputEvent e) override { if (e == InputEvent::Select) { selects++; return true; } return false; }
};
}

TEST(Keypad, UnconsumedSelectLongFallsBackToSelect) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  SelectOnlyApplet a; host.setRoot(&a);
  host.dispatch(InputEvent::SelectLong);   // over-long center press, no SelectLong handler
  EXPECT_EQ(1, a.selects);                  // falls back to Select instead of doing nothing
}

TEST(Keypad, BackRepeatPreferencePropagatesOnForegroundChange) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  RecordingSource src; host.addSource(&src);
  PlainApplet root; host.setRoot(&root);
  EXPECT_FALSE(src.holdRepeat);          // normal screen: Back must not repeat
  KeypadApplet kp; host.push(&kp);
  EXPECT_TRUE(src.holdRepeat);           // keypad wants Back to repeat-delete
  host.pop();
  EXPECT_FALSE(src.holdRepeat);          // revealed normal screen again
}

TEST(Keypad, HeldBackStopsAtEmptyFreshBackExits) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  PlainApplet root; host.setRoot(&root);
  KeypadApplet kp; host.push(&kp);
  EXPECT_EQ(2, host.depth());
  host.dispatch(InputEvent::Select);                  // type 'a' (focused on "abc")
  EXPECT_STREQ("a", kp.text());
  host.dispatch(InputEvent::Back, /*repeat=*/true);   // held delete -> empties buffer
  EXPECT_STREQ("", kp.text());
  EXPECT_EQ(2, host.depth());                          // still in the keypad
  host.dispatch(InputEvent::Back, /*repeat=*/true);   // held Back at empty must NOT exit
  EXPECT_EQ(2, host.depth());
  host.dispatch(InputEvent::Back, /*repeat=*/false);  // a fresh Back click exits
  EXPECT_EQ(1, host.depth());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
