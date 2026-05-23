#include <mishmesh/core/InputSources.h>

namespace mishmesh {

static Gesture toGesture(int buttonEvent) {
  switch (buttonEvent) {
    case BUTTON_EVENT_CLICK:        return Gesture::Click;
    case BUTTON_EVENT_LONG_PRESS:   return Gesture::LongPress;
    case BUTTON_EVENT_DOUBLE_CLICK: return Gesture::DoubleClick;
    case BUTTON_EVENT_TRIPLE_CLICK: return Gesture::TripleClick;
    default:                        return Gesture::None;
  }
}

bool ButtonGestureSource::poll(InputReport& out) {
  Gesture g = toGesture(_btn.check());
  if (g == Gesture::None) return false;
  InputEvent ev = mapGesture(_map, g);
  if (ev == InputEvent::None) return false;
  out.event = ev;
  out.ch = 0;
  return true;
}

bool DirectionalSource::poll(InputReport& out) {
  struct Item { MomentaryButton* btn; Direction dir; };
  Item items[] = {
    {&_up, Direction::Up},
    {&_down, Direction::Down},
    {&_left, Direction::Left},
    {&_right, Direction::Right},
    {&_press, Direction::Press},
  };
  for (Item& it : items) {
    if (it.btn->check() == BUTTON_EVENT_CLICK) {
      InputEvent ev = mapDirection(_map, it.dir);
      if (ev != InputEvent::None) {
        out.event = ev;
        out.ch = 0;
        return true;
      }
    }
  }
  return false;
}

}  // namespace mishmesh
