#include <gtest/gtest.h>
#include <mishmesh/core/AppletHost.h>
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

  explicit FakeApplet(const char* n) : Applet(n) {}
  void onStart(AppletContext&) override { started++; }
  void onForeground() override { foreground++; }
  void onBackground() override { background++; }
  void onStop() override { stopped++; }
  int onRender(Canvas&) override { rendered++; return renderDelay; }
  bool onInput(InputEvent ev) override { lastInput = ev; return consume; }
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

TEST(AppletHost, PopAtRootIsNoOp) {
  FakeDisplayDriver d;
  AppletHost host(&d, emptyCtx());
  FakeApplet root("root");
  host.setRoot(&root);
  host.pop();
  EXPECT_EQ(1, host.depth());
  EXPECT_EQ(0, root.stopped);
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
