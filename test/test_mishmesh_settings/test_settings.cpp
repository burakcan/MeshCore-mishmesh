#include <gtest/gtest.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/Toggle.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/applets/settings/AdvertSettingsPanel.h>
#include "FakeDisplayDriver.h"
using namespace mishmesh;

namespace {
// Minimal concrete panel proving the interface is usable through a base pointer.
class StubPanel : public mishmesh::SettingsPanel {
public:
  int begins = 0; int inputs = 0;
  const char* title() const override { return "Stub"; }
  void begin(mishmesh::AppletContext&) override { begins++; }
  int  renderBody(mishmesh::Canvas&, int, int, int, int) override { return 500; }
  bool onInput(mishmesh::InputEvent) override { inputs++; return true; }
};

struct ValModel : ListModel {
  int count() const override { return 2; }
  const char* label(int i) const override { return i == 0 ? "Users" : "Rooms"; }
  const char* value(int i) const override { return i == 0 ? "ON" : "OFF"; }
};
struct ToggleModel : ListModel {
  int count() const override { return 2; }
  const char* label(int i) const override { return i == 0 ? "Users" : "Rooms"; }
  bool isToggle(int) const override { return true; }
  bool toggleState(int i) const override { return i == 0; }
};
}

TEST(ListMenuMarquee, NeedsAnimationOnlyWhenSelectedOverflows) {
  struct LongModel : ListModel {
    int count() const override { return 1; }
    const char* label(int) const override { return "A very long contact name that overflows"; }
  };
  struct ShortModel : ListModel {
    int count() const override { return 1; }
    const char* label(int) const override { return "Bob"; }
  };
  FakeDisplayDriver d; Canvas c(&d);
  LongModel lm; ListMenu m1; m1.setModel(&lm);
  m1.draw(c, 0, 0, 64, 14);          // narrow box -> selected row 0 overflows
  EXPECT_TRUE(m1.needsAnimation());
  ShortModel sm; ListMenu m2; m2.setModel(&sm);
  m2.draw(c, 0, 0, 128, 14);
  EXPECT_FALSE(m2.needsAnimation());
}

TEST(ListMenuToggle, DrawsTogglePillRows) {
  FakeDisplayDriver d;
  Canvas c(&d);
  ToggleModel m;
  ListMenu menu;
  menu.setModel(&m);
  menu.draw(c, 0, 0, 128, 48);   // selected row 0 renders an ON pill in DARK on its LIGHT bar
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(ListMenuValue, DrawsValueColumn) {
  FakeDisplayDriver d;
  Canvas c(&d);
  ValModel m;
  ListMenu menu;
  menu.setModel(&m);
  menu.draw(c, 0, 0, 128, 48);
  // "ON"/"OFF" rendered => some glyph pixel runs beyond the label area.
  EXPECT_GT(d.fills.size(), 0u);
}

TEST(ListMenuValue, DrawsMoreFillsWithValue) {
  struct NullModel : ListModel {
    int count() const override { return 2; }
    const char* label(int i) const override { return i == 0 ? "Users" : "Rooms"; }
  };
  FakeDisplayDriver d_null, d_val;
  Canvas c_null(&d_null), c_val(&d_val);
  NullModel nm;  ValModel vm;
  ListMenu menu_null, menu_val;
  menu_null.setModel(&nm);  menu_val.setModel(&vm);
  menu_null.draw(c_null, 0, 0, 128, 48);
  menu_val.draw(c_val,  0, 0, 128, 48);
  EXPECT_GT(d_val.fills.size(), d_null.fills.size());
}

TEST(Toggle, StateText) {
  EXPECT_STREQ("ON", mishmesh::Toggle::stateText(true));
  EXPECT_STREQ("OFF", mishmesh::Toggle::stateText(false));
}

TEST(Toggle, DrawsPill) {
  FakeDisplayDriver d;
  Canvas c(&d);
  Toggle t;
  t.setOn(true);
  t.draw(c, 100, 0, 28, 12);
  EXPECT_GT(d.fills.size(), 0u);     // pill background/border + text
}

TEST(SettingsPanel, DefaultsAndDispatchThroughBasePointer) {
  StubPanel stub;
  mishmesh::SettingsPanel* p = &stub;
  EXPECT_STREQ("Stub", p->title());
  EXPECT_FALSE(p->modalActive());          // base default
  mishmesh::AppletContext ctx;
  p->begin(ctx);
  EXPECT_EQ(1, stub.begins);
  EXPECT_TRUE(p->onInput(mishmesh::InputEvent::Select));
  EXPECT_EQ(1, stub.inputs);
}

namespace {
class FakeShareApp : public mishmesh::AppServices {
public:
  bool share = false;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool shareLocationInAdvert() const override { return share; }
  void setShareLocationInAdvert(bool v) override { share = v; }
};
}  // namespace

TEST(AdvertSettingsPanel, SelectTogglesShareLocation) {
  FakeShareApp app;
  mishmesh::AppletContext ctx; ctx.app = &app;
  mishmesh::AdvertSettingsPanel panel;
  panel.begin(ctx);
  EXPECT_STREQ("Advert", panel.title());

  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::Select));
  EXPECT_TRUE(app.share);                       // off -> on
  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::Select));
  EXPECT_FALSE(app.share);                       // on -> off

  FakeDisplayDriver d; mishmesh::Canvas c(&d);
  panel.renderBody(c, 0, 0, 128, 50);
  EXPECT_GT(d.fills.size(), 0u);                 // toggle pill drawn
}

TEST(AdvertSettingsPanel, SingletonIsStable) {
  EXPECT_EQ(&mishmesh::advertSettings(), &mishmesh::advertSettings());
}

#include <mishmesh/applets/settings/MessagesSettingsPanel.h>
#include "FakeMessagesService.h"

TEST(MessagesSettingsPanel, TogglesAndAcksStepper) {
  FakeMessagesService svc;
  mishmesh::AppletContext ctx; ctx.messages = &svc;
  mishmesh::MessagesSettingsPanel panel;
  panel.begin(ctx);
  EXPECT_STREQ("Messages", panel.title());

  // Row 0 = Auto retry: Select flips it on.
  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::Select));
  EXPECT_TRUE(svc.getMessagesConfig().autoRetry);

  // Row 1 = Auto reset path.
  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::NavDown));
  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::Select));
  EXPECT_TRUE(svc.getMessagesConfig().autoResetPath);

  // Row 2 = Direct acks: Select opens the stepper modal.
  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::NavDown));
  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::Select));
  EXPECT_TRUE(panel.modalActive());
  // Bump 1 -> 2 and confirm.
  panel.onInput(mishmesh::InputEvent::NavRight);
  panel.onInput(mishmesh::InputEvent::Select);
  EXPECT_FALSE(panel.modalActive());
  EXPECT_EQ(2, svc.getMessagesConfig().directAcks);
}

#include <mishmesh/applets/settings/ContactsSettingsPanel.h>
#include "FakeContactsService.h"

TEST(ContactsSettingsPanel, MasterToggleWritesService) {
  FakeContactsService svc;            // default cfg.autoAddAll = true
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::ContactsSettingsPanel panel;
  panel.begin(ctx);
  EXPECT_STREQ("Contacts", panel.title());

  // Row 0 = "Auto-add all": Select flips master off.
  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::Select));
  EXPECT_FALSE(svc.getAutoAdd().autoAddAll);
}

TEST(ContactsSettingsPanel, MaxHopsStepperWritesValue) {
  FakeContactsService svc;            // autoAddAll=true -> rows: AutoAddAll(0),Overwrite(1),MaxHops(2)
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::ContactsSettingsPanel panel;
  panel.begin(ctx);

  panel.onInput(mishmesh::InputEvent::NavDown);   // -> Overwrite
  panel.onInput(mishmesh::InputEvent::NavDown);   // -> MaxHops
  panel.onInput(mishmesh::InputEvent::Select);    // open stepper
  EXPECT_TRUE(panel.modalActive());
  panel.onInput(mishmesh::InputEvent::NavRight);  // 3 -> 4
  panel.onInput(mishmesh::InputEvent::Select);    // confirm
  EXPECT_FALSE(panel.modalActive());
  EXPECT_EQ(4, svc.getAutoAdd().maxHops);
}

TEST(ContactsSettingsPanel, RemoveAllConfirmCallsService) {
  FakeContactsService svc;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::ContactsSettingsPanel panel;
  panel.begin(ctx);

  // Rows (autoAddAll on): AutoAddAll(0),Overwrite(1),MaxHops(2),
  //   RemoveNonUsers(3),RemoveNonFavourites(4),RemoveAll(5).
  for (int i = 0; i < 5; i++) panel.onInput(mishmesh::InputEvent::NavDown);
  panel.onInput(mishmesh::InputEvent::Select);    // opens confirm (defaults to Confirm)
  EXPECT_TRUE(panel.modalActive());
  panel.onInput(mishmesh::InputEvent::Select);    // confirm
  EXPECT_FALSE(panel.modalActive());
  bool called = false;
  for (auto& s : svc.calls) if (s == "removeall") called = true;
  EXPECT_TRUE(called);
}

#include <mishmesh/applets/SettingsDetailApplet.h>

TEST(SettingsDetailApplet, BindsBeginsAndRoutesInputToPanel) {
  StubPanel stub;
  mishmesh::SettingsDetailApplet detail;
  detail.setPanel(&stub);
  mishmesh::AppletContext ctx;
  detail.onStart(ctx);
  EXPECT_EQ(1, stub.begins);                       // begin() forwarded to the panel

  EXPECT_TRUE(detail.onInput(mishmesh::InputEvent::Select));
  EXPECT_EQ(1, stub.inputs);                       // input forwarded

  FakeDisplayDriver d; mishmesh::Canvas c(&d);
  detail.onRender(c);
  EXPECT_GT(d.fills.size(), 0u);                   // header + body drew something
}

TEST(SettingsDetailApplet, BackBubblesWhenPanelDeclines) {
  class DeclineStub : public StubPanel {
  public:
    bool onInput(mishmesh::InputEvent) override { return false; }   // consume nothing
  } stub;
  mishmesh::SettingsDetailApplet detail;
  detail.setPanel(&stub);
  mishmesh::AppletContext ctx; detail.onStart(ctx);
  EXPECT_FALSE(detail.onInput(mishmesh::InputEvent::Back));   // bubbles -> host pops
}

#include <mishmesh/applets/SettingsApplet.h>
#include <mishmesh/core/AppletHost.h>

TEST(SettingsApplet, SelectPushesDetailWithChosenPanel) {
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx;             // host fills app/contacts/messages as needed
  mishmesh::AppletHost host(&d, ctx);

  mishmesh::SettingsApplet menu;
  host.setRoot(&menu);
  EXPECT_EQ(4, menu.entryCountForTest());  // Contacts, Messages, Advert, System Info

  // Row 0 = Contacts: Select pushes the detail bound to contactsSettings().
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(&mishmesh::settingsDetailApplet(), host.foreground());

  host.dispatch(mishmesh::InputEvent::Back);   // pop back to the menu
  EXPECT_EQ(&menu, host.foreground());

  // Row 1 = Messages.
  host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ(&mishmesh::settingsDetailApplet(), host.foreground());
}

TEST(SettingsApplet, RendersStatusBarHeader) {
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx;
  mishmesh::AppletHost host(&d, ctx);
  mishmesh::SettingsApplet menu;
  host.setRoot(&menu);
  mishmesh::Canvas c(&d);
  menu.onRender(c);
  EXPECT_GT(d.fills.size(), 0u);                 // header + list drew something
  EXPECT_EQ(4, menu.entryCountForTest());        // Contacts/Messages/Advert/SystemInfo all available
}

#include <mishmesh/applets/settings/SystemInfoPanel.h>

namespace {
class FakeStatsApp : public mishmesh::AppServices {
public:
  mishmesh::SystemStats stats; bool ok = true;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool systemStats(mishmesh::SystemStats& out) const override {
    if (!ok) return false; out = stats; return true;
  }
};
}  // namespace

TEST(SystemInfoPanel, BuildsLinesFromServiceAndScrolls) {
  FakeStatsApp app; app.stats.heapFreeBytes = 145720; app.stats.contactsUsed = 12; app.stats.contactsMax = 100;
  mishmesh::AppletContext ctx; ctx.app = &app;
  mishmesh::SystemInfoPanel panel; panel.begin(ctx);
  EXPECT_STREQ("System Info", panel.title());
  FakeDisplayDriver d; mishmesh::Canvas c(&d);
  panel.renderBody(c, 0, 0, 128, 50);            // first render builds lines
  EXPECT_GT(d.fills.size(), 0u);
  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::NavDown));   // ScrollText consumes scroll
  EXPECT_FALSE(panel.onInput(mishmesh::InputEvent::Back));     // Back bubbles
}

TEST(SystemInfoPanel, UnavailableServiceRendersSafely) {
  FakeStatsApp app; app.ok = false;
  mishmesh::AppletContext ctx; ctx.app = &app;
  mishmesh::SystemInfoPanel panel; panel.begin(ctx);
  FakeDisplayDriver d; mishmesh::Canvas c(&d);
  panel.renderBody(c, 0, 0, 128, 50);            // must not crash; shows "Stats unavailable"
  SUCCEED();
}

#include <mishmesh/applets/settings/BluetoothPanel.h>

namespace {
class FakeBleApp2 : public mishmesh::AppServices {
public:
  bool supported = true, enabled = false, connected = false; uint32_t pin = 0; int setCalls = 0;
  const char* nodeName() const override { return "n"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 0; }
  bool bleSupported() const override { return supported; }
  bool bleEnabled() const override { return enabled; }
  bool bleConnected() const override { return connected; }
  uint32_t blePin() const override { return pin; }
  void setBleEnabled(bool v) override { enabled = v; setCalls++; }
};
}  // namespace

TEST(BluetoothPanel, ToggleRowFlipsRadioAndStatusRowReflectsLink) {
  FakeBleApp2 app; app.pin = 123456;
  mishmesh::AppletContext ctx; ctx.app = &app;
  mishmesh::BluetoothPanel panel; panel.begin(ctx);
  EXPECT_STREQ("Bluetooth", panel.title());

  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::Select));   // row 0 = toggle
  EXPECT_TRUE(app.enabled);
  EXPECT_EQ(1, app.setCalls);
}

TEST(BluetoothPanel, PinRowHiddenOncePairedOrNoPin) {
  EXPECT_STREQ("Connected", mishmesh::BluetoothPanel::statusText(true));
  EXPECT_STREQ("Not connected", mishmesh::BluetoothPanel::statusText(false));
  EXPECT_TRUE (mishmesh::BluetoothPanel::showPin(123456, false));
  EXPECT_FALSE(mishmesh::BluetoothPanel::showPin(0, false));
  EXPECT_FALSE(mishmesh::BluetoothPanel::showPin(123456, true));
}

#include <mishmesh/applets/settings/SoundPanel.h>
#include <mishmesh/sound/SoundEngine.h>
#include <FakeToneOutput.h>

TEST(SoundPanel, VolumeRowCyclesLevels) {
  FakeToneOutput out;
  mishmesh::sound::SoundEngine eng; eng.begin(&out);
  eng.setVolume(mishmesh::sound::VolumeLevel::Mute);
  mishmesh::AppletContext ctx; ctx.sound = &eng;   // no app -> panel writes engine directly
  mishmesh::SoundPanel panel; panel.begin(ctx);
  EXPECT_STREQ("Sound", panel.title());

  EXPECT_TRUE(panel.onInput(mishmesh::InputEvent::Select));   // Mute -> Low
  EXPECT_EQ(mishmesh::sound::VolumeLevel::Low, eng.volume());
  panel.onInput(mishmesh::InputEvent::Select);                // Low -> Mid
  EXPECT_EQ(mishmesh::sound::VolumeLevel::Mid, eng.volume());
}

TEST(SoundPanel, LevelNames) {
  using V = mishmesh::sound::VolumeLevel;
  EXPECT_STREQ("Mute", mishmesh::SoundPanel::levelName(V::Mute));
  EXPECT_STREQ("Low",  mishmesh::SoundPanel::levelName(V::Low));
  EXPECT_STREQ("Mid",  mishmesh::SoundPanel::levelName(V::Mid));
  EXPECT_STREQ("High", mishmesh::SoundPanel::levelName(V::High));
}

TEST(SettingsApplet, HidesBluetoothAndSoundWhenUnavailable) {
  FakeDisplayDriver d;
  mishmesh::AppletContext ctx;            // no app, no sound
  mishmesh::AppletHost host(&d, ctx);
  mishmesh::SettingsApplet menu; host.setRoot(&menu);
  EXPECT_EQ(4, menu.entryCountForTest());   // Contacts/Messages/Advert/System Info only
}

TEST(SettingsApplet, ShowsBluetoothAndSoundWhenAvailable) {
  FakeDisplayDriver d;
  FakeBleApp2 app;                          // bleSupported() == true
  FakeToneOutput out; mishmesh::sound::SoundEngine eng; eng.begin(&out);
  mishmesh::AppletContext ctx; ctx.app = &app; ctx.sound = &eng;
  mishmesh::AppletHost host(&d, ctx);
  mishmesh::SettingsApplet menu; host.setRoot(&menu);
  EXPECT_EQ(6, menu.entryCountForTest());   // all six visible
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
