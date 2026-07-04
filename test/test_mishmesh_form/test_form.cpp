#include <cstdio>
#include <gtest/gtest.h>
#include <mishmesh/applets/FormApplet.h>
#include <mishmesh/widgets/FormView.h>
#include <mishmesh/core/AppletHost.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

namespace {
int  g_submitCalls = 0;
bool g_submitRet   = true;
bool submitSpy(void*) { g_submitCalls++; return g_submitRet; }
bool rejectAll(const char*) { return false; }

// Minimal root so the form can be push()ed onto something.
struct StubApplet : Applet {
  StubApplet() : Applet("Stub") {}
  int onRender(Canvas&) override { return 1000; }
};
}  // namespace

TEST(FormApplet, NavMovesAcrossFieldsAndSubmit) {
  g_submitCalls = 0; g_submitRet = true;
  char name[16] = "hi"; char key[16] = "x";
  FormApplet::Field f[2] = {
    { "Name", name, sizeof(name), nullptr, "req" },
    { "Key",  key,  sizeof(key),  nullptr, "req" },
  };
  formApplet().configure("T", f, 2, submitSpy, nullptr);

  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  StubApplet stub; host.setRoot(&stub);
  host.push(&formApplet());
  host.loop(0);
  EXPECT_EQ(0, formApplet().focusedRowForTest());
  host.dispatch(InputEvent::NavDown);
  EXPECT_EQ(1, formApplet().focusedRowForTest());
  host.dispatch(InputEvent::NavDown);   // row 2 = Submit
  EXPECT_EQ(2, formApplet().focusedRowForTest());
}

TEST(FormApplet, InvalidFieldBlocksSubmit) {
  g_submitCalls = 0; g_submitRet = true;
  char name[16] = "";   // empty -> default validator fails
  FormApplet::Field f[1] = { { "Name", name, sizeof(name), nullptr, "Name required" } };
  formApplet().configure("T", f, 1, submitSpy, nullptr);

  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  StubApplet stub; host.setRoot(&stub);
  host.push(&formApplet());
  host.loop(0);
  host.dispatch(InputEvent::NavDown);   // -> Submit row
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(0, g_submitCalls);          // submit not reached
  EXPECT_EQ(2, host.depth());           // form still open (stub + form)
  EXPECT_EQ(0, formApplet().focusedRowForTest());   // failure focuses the bad field
}

TEST(FormApplet, ValidSubmitPopsWhenThunkReturnsTrue) {
  g_submitCalls = 0; g_submitRet = true;
  char name[16] = "ok";
  FormApplet::Field f[1] = { { "Name", name, sizeof(name), nullptr, "req" } };
  formApplet().configure("T", f, 1, submitSpy, nullptr);

  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  StubApplet stub; host.setRoot(&stub);
  host.push(&formApplet());
  host.loop(0);
  host.dispatch(InputEvent::NavDown);   // -> Submit
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(1, g_submitCalls);
  EXPECT_EQ(1, host.depth());           // popped back to stub
}

TEST(FormApplet, ValidSubmitStaysWhenThunkReturnsFalse) {
  g_submitCalls = 0; g_submitRet = false;
  char name[16] = "ok";
  FormApplet::Field f[1] = { { "Name", name, sizeof(name), nullptr, "req" } };
  formApplet().configure("T", f, 1, submitSpy, nullptr);

  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  StubApplet stub; host.setRoot(&stub);
  host.push(&formApplet());
  host.loop(0);
  host.dispatch(InputEvent::NavDown);
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(1, g_submitCalls);
  EXPECT_EQ(2, host.depth());           // stays open
}

TEST(FormApplet, CustomValidatorRejects) {
  g_submitCalls = 0; g_submitRet = true;
  char name[16] = "anything";
  FormApplet::Field f[1] = { { "Name", name, sizeof(name), rejectAll, "bad" } };
  formApplet().configure("T", f, 1, submitSpy, nullptr);

  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  StubApplet stub; host.setRoot(&stub);
  host.push(&formApplet());
  host.loop(0);
  host.dispatch(InputEvent::NavDown);
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(0, g_submitCalls);
  EXPECT_EQ(2, host.depth());
  EXPECT_EQ(0, formApplet().focusedRowForTest());   // failure focuses the bad field
}

TEST(FormApplet, SelectingFieldPushesKeypad) {
  char name[16] = "";
  FormApplet::Field f[1] = { { "Name", name, sizeof(name), nullptr, "req" } };
  formApplet().configure("T", f, 1, submitSpy, nullptr);

  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  StubApplet stub; host.setRoot(&stub);
  host.push(&formApplet());
  host.loop(0);
  host.dispatch(InputEvent::Select);    // field row 0 -> push keypad
  EXPECT_EQ(3, host.depth());           // stub + form + keypad
}

namespace {
void stepLabel(int v, char* out, uint16_t cap) { snprintf(out, cap, "v=%d", v); }
}

TEST(FormApplet, StepperFieldOpensModalAndWritesValue) {
  g_submitCalls = 0; g_submitRet = true;
  int hops = 1;
  FormApplet::Field f[1] = {
    { "Size", nullptr, 0, nullptr, nullptr, nullptr,
      FormApplet::Stepper, &hops, 1, 3, stepLabel },
  };
  formApplet().configure("T", f, 1, submitSpy, nullptr);

  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  StubApplet stub; host.setRoot(&stub);
  host.push(&formApplet());
  host.loop(0);

  // Select on the stepper field opens the modal (no keypad pushed: depth stays 2).
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(2, host.depth());
  host.dispatch(InputEvent::NavRight);   // 1 -> 2
  host.dispatch(InputEvent::NavRight);   // 2 -> 3
  host.dispatch(InputEvent::Select);     // confirm
  EXPECT_EQ(3, hops);                     // caller-owned int updated

  // Now move to the submit button and submit succeeds.
  host.dispatch(InputEvent::NavDown);    // field 0 -> button
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(1, g_submitCalls);
  EXPECT_EQ(1, host.depth());            // popped back to stub
}

// ---- FormView unit tests ----

TEST(FormView, DrawDoesNotCrash) {
  FakeDisplayDriver d;
  Canvas c(&d);
  FormRow rows[2] = { { "Name", "Alice" }, { "Key", "abc" } };
  FormView v;
  // draw with button
  v.draw(c, 0, 10, 128, 50, rows, 2, "Submit");
  // draw without button (submitLabel == nullptr)
  v.draw(c, 0, 10, 128, 50, rows, 2, nullptr);
}

TEST(FormView, FocusAndSetFocus) {
  FormView v;
  EXPECT_EQ(0, v.focus());
  v.setFocus(2);
  EXPECT_EQ(2, v.focus());
}

TEST(FormView, ResetClearsFocus) {
  FormView v;
  v.setFocus(3);
  v.reset();
  EXPECT_EQ(0, v.focus());
}

TEST(FormView, NavDownAdvancesToButton) {
  FormView v;  // focus starts at 0
  // 2 fields (n=2) + button (hasButton=true) => max focus = 2
  bool r0 = v.onInput(InputEvent::NavDown, 2, true);  // 0 -> 1
  EXPECT_TRUE(r0);
  EXPECT_EQ(1, v.focus());
  bool r1 = v.onInput(InputEvent::NavDown, 2, true);  // 1 -> 2 (button)
  EXPECT_TRUE(r1);
  EXPECT_EQ(2, v.focus());
  // already at max; stays
  bool r2 = v.onInput(InputEvent::NavDown, 2, true);
  EXPECT_TRUE(r2);
  EXPECT_EQ(2, v.focus());
}

TEST(FormView, NavDownClampsWithoutButton) {
  FormView v;
  // 2 fields, no button => max focus = 1
  v.onInput(InputEvent::NavDown, 2, false);  // 0 -> 1
  EXPECT_EQ(1, v.focus());
  v.onInput(InputEvent::NavDown, 2, false);  // clamped at 1
  EXPECT_EQ(1, v.focus());
}

TEST(FormView, NavUpClampsAtZero) {
  FormView v;
  bool r = v.onInput(InputEvent::NavUp, 2, true);
  EXPECT_TRUE(r);
  EXPECT_EQ(0, v.focus());   // already at 0, no change
}

TEST(FormView, NavUpDecrements) {
  FormView v;
  v.setFocus(2);
  v.onInput(InputEvent::NavUp, 2, true);
  EXPECT_EQ(1, v.focus());
  v.onInput(InputEvent::NavUp, 2, true);
  EXPECT_EQ(0, v.focus());
}

TEST(FormView, SelectNotConsumed) {
  FormView v;
  EXPECT_FALSE(v.onInput(InputEvent::Select, 2, true));
}

TEST(FormView, OtherEventsNotConsumed) {
  FormView v;
  EXPECT_FALSE(v.onInput(InputEvent::Back, 1, true));
  EXPECT_FALSE(v.onInput(InputEvent::Cancel, 1, true));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
