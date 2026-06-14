// test/test_mishmesh_chatnotify/test_chatnotify.cpp
#include <gtest/gtest.h>
#include <mishmesh/applets/ChatNotifyApplet.h>
#include <mishmesh/core/MessagesService.h>
#include "mocks/FakeDisplayDriver.h"
using namespace mishmesh;

// Minimal MessagesService recording the last setNotifyLevel call.
struct FakeMsgSvc : MessagesService {
  NotifyLevel level = NotifyLevel::All;
  NotifyLevel lastSet = NotifyLevel::All;
  int convoCount() const override { return 0; }
  bool getConvo(int, ConvoView&) const override { return false; }
  uint16_t totalUnread() const override { return 0; }
  uint16_t totalNotifyUnread() const override { return 0; }
  int messageCount(const ConvoKey&) const override { return 0; }
  bool getMessage(const ConvoKey&, int, MessageView&) const override { return false; }
  void setActiveConvo(const ConvoKey&) override {}
  void clearActiveConvo() override {}
  int repeatCount(const ConvoKey&, int) const override { return 0; }
  bool getRepeat(const ConvoKey&, int, int, RepeatView&) const override { return false; }
  bool resolveHop(uint8_t, const char*&, uint8_t&) const override { return false; }
  void deleteMessage(const ConvoKey&, int) override {}
  void clearConvo(const ConvoKey&) override {}
  void deleteConvo(const ConvoKey&) override {}
  void markUnread(const ConvoKey&) override {}
  bool sendText(const ConvoKey&, const char*) override { return false; }
  uint32_t seq() const override { return 0; }
  NotifyLevel notifyLevel(const ConvoKey&) const override { return level; }
  void setNotifyLevel(const ConvoKey&, NotifyLevel l) override { lastSet = l; }
};

static AppletContext makeCtx(FakeMsgSvc& svc) {
  AppletContext ctx; ctx.messages = &svc; return ctx;
}

TEST(ChatNotify, DmShowsTwoRows) {
  FakeMsgSvc svc;
  ConvoKey k = directKey((const uint8_t*)"ALICE!");
  auto& a = chatNotifyApplet();
  a.setTarget(k, "Alice");
  AppletContext ctx = makeCtx(svc);
  a.onStart(ctx);
  EXPECT_EQ(2, a.count());
}

TEST(ChatNotify, ChannelShowsThreeRows) {
  FakeMsgSvc svc;
  ConvoKey k = channelKey(3);
  auto& a = chatNotifyApplet();
  a.setTarget(k, "#general");
  AppletContext ctx = makeCtx(svc);
  a.onStart(ctx);
  EXPECT_EQ(3, a.count());
}

TEST(ChatNotify, SelectPersistsLevel) {
  FakeMsgSvc svc;
  ConvoKey k = channelKey(3);
  auto& a = chatNotifyApplet();
  a.setTarget(k, "#general");
  AppletContext ctx = makeCtx(svc);
  a.onStart(ctx);
  a.onInput(InputEvent::NavDown);   // row 0 (All) -> row 1 (Mentions only)
  a.onInput(InputEvent::Select);
  EXPECT_EQ(NotifyLevel::MentionsOnly, svc.lastSet);
}

TEST(ChatNotify, RadioMarksExactlyTheActiveLevel) {
  FakeMsgSvc svc;
  svc.level = NotifyLevel::MentionsOnly;
  ConvoKey k = channelKey(3);
  auto& a = chatNotifyApplet();
  a.setTarget(k, "#general");
  AppletContext ctx = makeCtx(svc);
  a.onStart(ctx);
  EXPECT_FALSE(a.radioOn(0));   // All
  EXPECT_TRUE(a.radioOn(1));    // Mentions only (active)
  EXPECT_FALSE(a.radioOn(2));   // Mute
  EXPECT_TRUE(a.isRadio(0));    // single-select rows, not toggles
  EXPECT_FALSE(a.isToggle(0));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
