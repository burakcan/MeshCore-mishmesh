#include <gtest/gtest.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/Canvas.h>

#include <map>
#include <string>
#include <vector>

using namespace mishmesh;

namespace {

// In-memory AppletStorage (mirrors the adapter's key->blob contract).
struct MemStorage : AppletStorage {
  std::map<std::string, std::vector<uint8_t>> kv;
  uint8_t load(const char* key, uint8_t* dst, uint8_t cap) override {
    auto it = kv.find(key);
    if (it == kv.end()) return 0;
    uint8_t n = (uint8_t)std::min<size_t>(cap, it->second.size());
    memcpy(dst, it->second.data(), n);
    return n;
  }
  bool save(const char* key, const uint8_t* src, uint8_t len) override {
    kv[key] = std::vector<uint8_t>(src, src + len);
    return true;
  }
};

struct NullApplet : Applet {
  NullApplet() : Applet("null") {}
  int onRender(Canvas&) override { return 1000; }
};

NullApplet a1, a2, a3;
AppletRegistration r1{&a1, "Contacts", 0, Placement::AppMenu, 1, nullptr};
AppletRegistration r2{&a2, "Messages", 0, Placement::AppMenu, 0, nullptr};
AppletRegistration r3{&a3, "Hidden",   0, Placement::LaunchOnly, 2, nullptr};

void primeRegistry() {
  resetRegistry();
  registerApplet(&r1);
  registerApplet(&r2);
  registerApplet(&r3);
}

}  // namespace

TEST(UiPrefs, DefaultsWithoutStorage) {
  primeRegistry();
  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);
  EXPECT_FALSE(uiPrefs().battShowPercent());
  EXPECT_STREQ("Contacts", uiPrefs().quickActionLabel(UiPrefs::SLOT_LEFT));
  EXPECT_STREQ("Messages", uiPrefs().quickActionLabel(UiPrefs::SLOT_RIGHT));
  ASSERT_NE(nullptr, uiPrefs().quickAction(UiPrefs::SLOT_LEFT));
  EXPECT_EQ(&a1, uiPrefs().quickAction(UiPrefs::SLOT_LEFT)->applet);
}

TEST(UiPrefs, PersistsAndReloads) {
  primeRegistry();
  MemStorage mem;
  uiPrefs().resetForTest();
  uiPrefs().begin(&mem);
  uiPrefs().setBattShowPercent(true);
  uiPrefs().setQuickAction(UiPrefs::SLOT_RIGHT, "Contacts");

  uiPrefs().resetForTest();          // simulate reboot
  uiPrefs().begin(&mem);
  EXPECT_TRUE(uiPrefs().battShowPercent());
  EXPECT_STREQ("Contacts", uiPrefs().quickActionLabel(UiPrefs::SLOT_RIGHT));
  EXPECT_EQ(&a1, uiPrefs().quickAction(UiPrefs::SLOT_RIGHT)->applet);
}

TEST(UiPrefs, DarkModePersists) {
  primeRegistry();
  MemStorage mem;
  uiPrefs().resetForTest();
  uiPrefs().begin(&mem);
  EXPECT_TRUE(uiPrefs().darkMode());                // dark is the default
  uiPrefs().setDarkMode(false);

  uiPrefs().resetForTest();                         // simulate reboot
  uiPrefs().begin(&mem);
  EXPECT_FALSE(uiPrefs().darkMode());
}

TEST(UiPrefs, UnknownLabelFallsBackToDefault) {
  primeRegistry();
  MemStorage mem;
  uiPrefs().resetForTest();
  uiPrefs().begin(&mem);
  uiPrefs().setQuickAction(UiPrefs::SLOT_LEFT, "Gone applet");
  // Stored label doesn't resolve -> default slot applet.
  ASSERT_NE(nullptr, uiPrefs().quickAction(UiPrefs::SLOT_LEFT));
  EXPECT_EQ(&a1, uiPrefs().quickAction(UiPrefs::SLOT_LEFT)->applet);
}

TEST(UiPrefs, LaunchOnlyNeverResolves) {
  primeRegistry();
  MemStorage mem;
  uiPrefs().resetForTest();
  uiPrefs().begin(&mem);
  uiPrefs().setQuickAction(UiPrefs::SLOT_LEFT, "Hidden");
  EXPECT_EQ(&a1, uiPrefs().quickAction(UiPrefs::SLOT_LEFT)->applet);  // default
}

TEST(UiPrefs, NullWhenNothingResolves) {
  resetRegistry();   // empty registry: even defaults miss
  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);
  EXPECT_EQ(nullptr, uiPrefs().quickAction(UiPrefs::SLOT_LEFT));
  EXPECT_EQ(nullptr, uiPrefs().quickAction(UiPrefs::SLOT_RIGHT));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
