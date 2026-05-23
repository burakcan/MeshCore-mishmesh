#include <gtest/gtest.h>
#include <mishmesh/core/InputMapping.h>

using namespace mishmesh;

TEST(MapGesture, MapsEachGestureToConfiguredEvent) {
  GestureMap m;
  m.click = InputEvent::NavDown;
  m.doubleClick = InputEvent::Select;
  m.longPress = InputEvent::Back;
  // tripleClick left as default (None)

  EXPECT_EQ(InputEvent::NavDown, mapGesture(m, Gesture::Click));
  EXPECT_EQ(InputEvent::Select,  mapGesture(m, Gesture::DoubleClick));
  EXPECT_EQ(InputEvent::Back,    mapGesture(m, Gesture::LongPress));
  EXPECT_EQ(InputEvent::None,    mapGesture(m, Gesture::TripleClick));
  EXPECT_EQ(InputEvent::None,    mapGesture(m, Gesture::None));
}

TEST(MapDirection, DefaultMapMatchesNavDirections) {
  DirectionalMap m;  // defaults: up->NavUp, ..., press->Select
  EXPECT_EQ(InputEvent::NavUp,    mapDirection(m, Direction::Up));
  EXPECT_EQ(InputEvent::NavDown,  mapDirection(m, Direction::Down));
  EXPECT_EQ(InputEvent::NavLeft,  mapDirection(m, Direction::Left));
  EXPECT_EQ(InputEvent::NavRight, mapDirection(m, Direction::Right));
  EXPECT_EQ(InputEvent::Select,   mapDirection(m, Direction::Press));
}

TEST(MapDirection, RespectsCustomPressEvent) {
  DirectionalMap m;
  m.press = InputEvent::Back;
  EXPECT_EQ(InputEvent::Back, mapDirection(m, Direction::Press));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
