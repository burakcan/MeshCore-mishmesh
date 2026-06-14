#include <cstdio>
#include <gtest/gtest.h>
#include <mishmesh/applets/FormApplet.h>
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
