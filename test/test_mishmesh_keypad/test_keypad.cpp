#include <gtest/gtest.h>
#include <string>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/EmojiCatalog.h>
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
  k.cycleMode();                             // abc -> Abc (one-shot capitalize)
  EXPECT_EQ(KeypadApplet::Mode::Shift, k.mode());
  EXPECT_STREQ("ABC", k.cellLabel(0, 1));    // Shift shows caps for the pending letter
  EXPECT_STREQ("Ab",  k.cellLabel(2, 3));
  k.cycleMode();                             // Abc -> ABC
  EXPECT_EQ(KeypadApplet::Mode::Upper, k.mode());
  EXPECT_STREQ("ABC", k.cellLabel(0, 1));
  EXPECT_STREQ("AB",  k.cellLabel(2, 3));
  k.cycleMode();                             // ABC -> 123
  EXPECT_EQ(KeypadApplet::Mode::Num, k.mode());
  EXPECT_STREQ("2", k.cellLabel(0, 1));      // abc cell -> digit 2
  EXPECT_STREQ("0", k.cellLabel(3, 0));      // sym cell -> digit 0 in Num mode
  EXPECT_STREQ("12", k.cellLabel(2, 3));
  k.cycleMode();                             // 123 -> abc
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

TEST(Keypad, DelCellDeletesBeforeCursor) {
  KeypadApplet k; Harness h(&k);                  // opens focused on "abc"
  k.onInput(InputEvent::Select);                  // 'a'
  k.onInput(InputEvent::NavRight);                // commit, focus def
  k.onInput(InputEvent::Select);                  // 'd' -> "ad"
  k.setFocusForTest(0, 3);                        // DEL cell (backspace lives here now)
  k.onInput(InputEvent::Select);                  // delete before cursor
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
  k.cycleMode(); k.cycleMode(); k.cycleMode();    // Lower->Shift->Upper->Num (focus stays on (0,1) = "2")
  k.onInput(InputEvent::Select);
  k.onInput(InputEvent::Select);                  // single-char group: two '2's
  EXPECT_STREQ("22", k.text());
}

TEST(Keypad, AbcModeCapitalizesFirstLetterThenReverts) {
  KeypadApplet k; Harness h(&k);             // opens focused on "abc"
  k.cycleMode();                             // -> Abc
  EXPECT_EQ(KeypadApplet::Mode::Shift, k.mode());
  k.onInput(InputEvent::Select);             // first letter -> capital 'A', still armed (pending)
  EXPECT_STREQ("A", k.text());
  EXPECT_EQ(KeypadApplet::Mode::Shift, k.mode());
  k.onInput(InputEvent::NavRight);           // commit the pending letter -> reverts to Lower
  EXPECT_EQ(KeypadApplet::Mode::Lower, k.mode());
  k.onInput(InputEvent::Select);             // next letter (def cell) is lowercase
  EXPECT_STREQ("Ad", k.text());
}

TEST(Keypad, AbcModeMultiTapStaysCapitalWithinGroup) {
  KeypadApplet k; Harness h(&k);             // opens focused on "abc"
  k.cycleMode();                             // -> Abc
  k.onInput(InputEvent::Select);             // 'A'
  k.onInput(InputEvent::Select);             // cycle same group, still shift -> 'B'
  EXPECT_STREQ("B", k.text());
  EXPECT_EQ(KeypadApplet::Mode::Shift, k.mode());
}

TEST(Keypad, AbcModeNavWithoutTypingKeepsShiftArmed) {
  KeypadApplet k; Harness h(&k);             // opens focused on "abc"
  k.cycleMode();                             // -> Abc
  k.onInput(InputEvent::NavRight);           // just moving focus, no letter entered
  EXPECT_EQ(KeypadApplet::Mode::Shift, k.mode());
  k.onInput(InputEvent::Select);             // def cell -> capital 'D'
  EXPECT_STREQ("D", k.text());
}

TEST(Keypad, LongPressTypesDigitFromLetterCell) {
  KeypadApplet k; Harness h(&k);             // opens focused on "abc" = cell idx 1 -> '2'
  k.onInput(InputEvent::SelectLong);
  EXPECT_STREQ("2", k.text());
  k.setFocusForTest(2, 2);                    // cell idx 8 -> '9'
  k.onInput(InputEvent::SelectLong);
  EXPECT_STREQ("29", k.text());
  k.setFocusForTest(3, 0);                    // sym/'0' cell -> '0'
  k.onInput(InputEvent::SelectLong);
  EXPECT_STREQ("290", k.text());
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

TEST(Keypad, KeypadDoesNotRequestBackRepeat) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  RecordingSource src; host.addSource(&src);
  PlainApplet root; host.setRoot(&root);
  EXPECT_FALSE(src.holdRepeat);          // normal screen: Back must not repeat
  KeypadApplet kp; host.push(&kp);
  EXPECT_FALSE(src.holdRepeat);          // Back is exit now, not repeat-delete
}

TEST(Keypad, FreshBackOnEmptyExits) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  PlainApplet root; host.setRoot(&root);
  KeypadApplet kp; host.push(&kp);
  EXPECT_EQ(2, host.depth());
  host.dispatch(InputEvent::Back);                    // empty (clean) -> exit straight away
  EXPECT_EQ(1, host.depth());
}

namespace {
struct ConfirmCapture {
  std::string text;
  int calls = 0;
  static void cb(void* ctx, const char* t) {
    auto* self = static_cast<ConfirmCapture*>(ctx);
    self->text = t ? t : "";
    self->calls++;
  }
};
}

TEST(Keypad, ConfirmFiresCallbackWithTypedText) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);

  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1] = {0};
  ConfirmCapture cap;
  k.configure(buf, KeypadApplet::KP_MAX, "Message", &ConfirmCapture::cb, &cap);
  host.push(&k);
  EXPECT_EQ(2, host.depth());

  k.onInput(InputEvent::Select);   // 'a' (opens focused on "abc")
  k.setFocusForTest(3, 3);         // OK cell
  k.onInput(InputEvent::Select);   // confirm

  EXPECT_EQ(1, cap.calls);
  EXPECT_EQ("a", cap.text);
  EXPECT_EQ(1, host.depth());      // popped back to root
}

TEST(Keypad, StandaloneConfirmDoesNotCallback) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);

  KeypadApplet k;                  // no configure() -> standalone, no callback
  host.push(&k);
  k.onInput(InputEvent::Select);   // 'a'
  k.setFocusForTest(3, 3);
  k.onInput(InputEvent::Select);   // confirm
  EXPECT_EQ(1, host.depth());      // still pops, as before
}

// ---- working-buffer model + discard-confirm (new behavior) -----------------

TEST(Keypad, ConfiguredSeedsWorkingBufferFromSource) {
  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  k.configure(buf, KeypadApplet::KP_MAX, "T");
  EXPECT_STREQ("hi", k.text());          // working copy seeded from source
  EXPECT_EQ(2, k.cursor());
}

TEST(Keypad, EditsDoNotTouchSourceBeforeOk) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);
  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  k.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&k);
  k.onInput(InputEvent::Select);         // 'a' appended to working copy -> "hia"
  EXPECT_STREQ("hia", k.text());
  EXPECT_STREQ("hi", buf);               // source untouched until OK
}

TEST(Keypad, OkCommitsWorkingBufferToSource) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);
  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1]; buf[0] = 0;
  k.configure(buf, KeypadApplet::KP_MAX, "T");   // no onConfirm: write-back path (FormApplet)
  host.push(&k);
  k.onInput(InputEvent::Select);         // 'a'
  k.setFocusForTest(3, 3);               // OK cell
  k.onInput(InputEvent::Select);         // confirm
  EXPECT_STREQ("a", buf);                // committed to source
  EXPECT_EQ(1, host.depth());            // popped
}

TEST(Keypad, BackWhenCleanPopsWithoutDialog) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);
  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  k.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&k);
  host.dispatch(InputEvent::Back);       // unchanged -> exit immediately
  EXPECT_EQ(1, host.depth());            // popped, no discard dialog
}

TEST(Keypad, BackWhenDirtyOpensDiscardConfirm) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);
  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  k.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&k);
  k.onInput(InputEvent::Select);         // edit -> dirty
  host.dispatch(InputEvent::Back);
  EXPECT_EQ(2, host.depth());            // NOT popped: discard dialog is up
}

TEST(Keypad, BackLongAlsoConfirmsWhenDirty) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);
  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  k.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&k);
  k.onInput(InputEvent::Select);         // dirty
  host.dispatch(InputEvent::BackLong);   // behaves like Back: confirm, don't bypass
  EXPECT_EQ(2, host.depth());
}

TEST(Keypad, DiscardConfirmedPopsWithoutCommitting) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);
  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  k.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&k);
  k.onInput(InputEvent::Select);         // "hia", dirty
  host.dispatch(InputEvent::Back);       // discard dialog (default sel = Confirm)
  host.dispatch(InputEvent::Select);     // confirm discard
  EXPECT_EQ(1, host.depth());            // popped
  EXPECT_STREQ("hi", buf);               // source never written
}

TEST(Keypad, DiscardCancelledStaysAndKeepsText) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static KeypadApplet root; host.setRoot(&root);
  KeypadApplet k;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  k.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&k);
  k.onInput(InputEvent::Select);         // "hia", dirty
  host.dispatch(InputEvent::Back);       // discard dialog (default sel = Confirm)
  host.dispatch(InputEvent::NavLeft);    // move to Cancel
  host.dispatch(InputEvent::Select);     // dismiss dialog without discarding
  EXPECT_EQ(2, host.depth());            // still editing
  EXPECT_STREQ("hia", k.text());         // text preserved
}

TEST(KeypadNumeric, LocksModeAndTypesDotAndMinus) {
  mishmesh::KeypadApplet& k = mishmesh::keypadApplet();
  char buf[16] = {0};
  k.configureNumeric(buf, sizeof(buf) - 1, "Frequency (MHz)");
  mishmesh::AppletContext ctx;           // host not needed for typing
  k.onStart(ctx);
  EXPECT_EQ(k.mode(), mishmesh::KeypadApplet::Mode::Num);

  k.cycleMode();                          // must be a no-op in numeric mode
  EXPECT_EQ(k.mode(), mishmesh::KeypadApplet::Mode::Num);
  k.toggleSymPage();
  EXPECT_FALSE(k.symPage());

  // Type "9" then "." then "1".
  // Char cells: (r<3, c<3) -> groupAt(r*3+c). In Num mode groupAt = {"1".."9"}.
  // (2,2) -> groupAt(8) = "9"; (0,0) -> groupAt(0) = "1".
  // Dot cell: shift cell at (r==2, c==3) -> '.' in numeric mode.
  k.setFocusForTest(2, 2);                // char cell 8 -> group "9"
  k.onInput(mishmesh::InputEvent::Select);
  k.setFocusForTest(2, 3);                // shift/dot cell -> '.' in numeric
  k.onInput(mishmesh::InputEvent::Select);
  k.setFocusForTest(0, 0);                // char cell 0 -> group "1"
  k.onInput(mishmesh::InputEvent::Select);
  EXPECT_STREQ(k.text(), "9.1");

  k.onStop();
}

TEST(KeypadUtf8, EncodesAllLengths) {
  char b[5];
  EXPECT_EQ(1, KeypadApplet::utf8Encode(0x41, b));     EXPECT_STREQ("A", b);
  EXPECT_EQ(2, KeypadApplet::utf8Encode(0xE9, b));     // é U+00E9
  EXPECT_EQ((char)0xC3, b[0]); EXPECT_EQ((char)0xA9, b[1]); EXPECT_EQ(0, b[2]);
  EXPECT_EQ(3, KeypadApplet::utf8Encode(0x2764, b));   // ❤ U+2764
  EXPECT_EQ((char)0xE2, b[0]); EXPECT_EQ((char)0x9D, b[1]); EXPECT_EQ((char)0xA4, b[2]);
  EXPECT_EQ(4, KeypadApplet::utf8Encode(0x1F600, b));  // 😀 U+1F600
  EXPECT_EQ((char)0xF0, b[0]); EXPECT_EQ((char)0x9F, b[1]);
  EXPECT_EQ((char)0x98, b[2]); EXPECT_EQ((char)0x80, b[3]); EXPECT_EQ(0, b[4]);
}

TEST(KeypadUtf8, EncodeRoundTripLengths) {
  char b[5];
  EXPECT_EQ(4, KeypadApplet::utf8Encode(0x1F44D, b)); // 👍
  EXPECT_EQ(4, (int)strlen(b));
}

// A 13-entry stand-in catalog forces 2 pages (12/page).
static const uint32_t kPickCat[] = {
  0x1F600,0x1F604,0x1F602,0x1F642,0x1F60A,0x1F609,0x1F60D,0x1F618,0x1F61C,0x1F61B,
  0x1F60E,0x1F914,   // 12 on page 0
  0x2764             // 1 on page 1
};

struct KeypadEmoji : ::testing::Test {
  void TearDown() override { setEmojiCatalog(nullptr, 0); }
};

TEST_F(KeypadEmoji, DormantWithoutCatalog) {
  setEmojiCatalog(nullptr, 0);
  KeypadApplet k;
  k.cycleBottomLeft();                       // letters -> sym
  EXPECT_TRUE(k.symPage());
  EXPECT_FALSE(k.emojiPage());
  k.cycleBottomLeft();                       // sym -> letters (emoji skipped)
  EXPECT_FALSE(k.symPage());
  EXPECT_FALSE(k.emojiPage());
}

TEST_F(KeypadEmoji, CycleReachesEmojiAndBack) {
  setEmojiCatalog(kPickCat, 13);
  KeypadApplet k;
  k.cycleBottomLeft(); EXPECT_TRUE(k.symPage());
  k.cycleBottomLeft(); EXPECT_TRUE(k.emojiPage()); EXPECT_FALSE(k.symPage());
  EXPECT_EQ(2, k.emojiPageCount());
  k.cycleBottomLeft(); EXPECT_FALSE(k.emojiPage());   // back to letters
}

TEST_F(KeypadEmoji, PageLabelsAndWrap) {
  setEmojiCatalog(kPickCat, 13);
  KeypadApplet k;
  k.cycleBottomLeft(); k.cycleBottomLeft();  // into emoji, page 0
  EXPECT_STREQ("\xF0\x9F\x98\x80", k.cellLabel(0, 0));   // cell0 = 😀
  k.nextEmojiPage();                          // page 1
  EXPECT_EQ(1, k.emojiPageIndex());
  EXPECT_STREQ("\xE2\x9D\xA4", k.cellLabel(0, 0));       // cell0 = ❤
  EXPECT_STREQ("", k.cellLabel(0, 1));                   // past the end -> blank
  k.nextEmojiPage(); EXPECT_EQ(0, k.emojiPageIndex());   // wraps to page 0
  k.prevEmojiPage(); EXPECT_EQ(1, k.emojiPageIndex());   // wraps back
}

TEST_F(KeypadEmoji, SelectInsertsUtf8) {
  setEmojiCatalog(kPickCat, 13);
  KeypadApplet k;
  k.cycleBottomLeft(); k.cycleBottomLeft();  // emoji, page 0
  k.insertEmojiCell(0);                       // insert 😀
  EXPECT_STREQ("\xF0\x9F\x98\x80", k.text());
  EXPECT_EQ(4, (int)k.length());
  k.insertEmojiCell(11);                      // insert 🤔 (0x1F914)
  EXPECT_EQ(8, (int)k.length());
  EXPECT_TRUE(k.emojiPage());                 // stays in emoji mode
}

// The bottom-left label shows the NEXT state so emoji mode is discoverable.
TEST_F(KeypadEmoji, BottomLeftLabelShowsNextState) {
  setEmojiCatalog(kPickCat, 13);
  KeypadApplet k;
  EXPECT_STREQ("sym", k.cellLabel(3, 0));   // letters -> next is sym
  k.cycleBottomLeft();
  EXPECT_STREQ("emo", k.cellLabel(3, 0));   // sym -> next is emoji (catalog present)
  k.cycleBottomLeft();
  EXPECT_STREQ("abc", k.cellLabel(3, 0));   // emoji -> next is letters
}

TEST_F(KeypadEmoji, SymLabelStaysAbcWithoutCatalog) {
  setEmojiCatalog(nullptr, 0);
  KeypadApplet k;
  k.cycleBottomLeft();                      // letters -> sym
  EXPECT_STREQ("abc", k.cellLabel(3, 0));   // no catalog -> next is letters
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
