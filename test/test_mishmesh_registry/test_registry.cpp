#include <gtest/gtest.h>
#include <mishmesh/core/AppletRegistry.h>

using namespace mishmesh;

// Sentinel non-null Applet pointers (registry only stores the pointer).
static Applet* const A1 = reinterpret_cast<Applet*>(0x1);
static Applet* const A2 = reinterpret_cast<Applet*>(0x2);

TEST(Registry, RegistersAndWalksList) {
  resetRegistry();
  static AppletRegistration r1{A1, "one", nullptr, Placement::AppMenu, 0, nullptr};
  static AppletRegistration r2{A2, "two", nullptr, Placement::LaunchOnly, 0, nullptr};
  registerApplet(&r1);
  registerApplet(&r2);

  int count = 0;
  for (AppletRegistration* r = registeredApplets(); r != nullptr; r = r->next) count++;
  EXPECT_EQ(2, count);
}

TEST(Registry, CanFilterByPlacement) {
  resetRegistry();
  static AppletRegistration r1{A1, "menu", nullptr, Placement::AppMenu, 5, nullptr};
  static AppletRegistration r2{A2, "hidden", nullptr, Placement::LaunchOnly, 0, nullptr};
  registerApplet(&r1);
  registerApplet(&r2);

  int menuCount = 0;
  const AppletRegistration* found = nullptr;
  for (AppletRegistration* r = registeredApplets(); r != nullptr; r = r->next) {
    if (r->placement == Placement::AppMenu) { menuCount++; found = r; }
  }
  EXPECT_EQ(1, menuCount);
  ASSERT_NE(nullptr, found);
  EXPECT_STREQ("menu", found->label);
  EXPECT_EQ(5, found->order);
}

TEST(Registry, ResetClearsList) {
  resetRegistry();
  EXPECT_EQ(nullptr, registeredApplets());
}

// Exercises the convenience macro (registers at first call via function-local static).
static void registerSampleViaMacro() {
  MISHMESH_REGISTER_APPLET(A1, ::mishmesh::Placement::AppMenu, "macro", 0);
}

TEST(Registry, MacroRegistersApplet) {
  resetRegistry();
  registerSampleViaMacro();
  ASSERT_NE(nullptr, registeredApplets());
  EXPECT_STREQ("macro", registeredApplets()->label);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
