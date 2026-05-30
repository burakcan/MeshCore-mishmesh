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
  uint32_t now = millis();
  for (int i = 0; i < 5; i++) {
    Item& it = items[i];
    // Debounce: accept a level change only after it has held steady, so contact
    // bounce on press/release can't fire a spurious second step.
    bool raw = it.btn->isPressed();
    if (raw != _rawPressed[i]) { _rawPressed[i] = raw; _rawSince[i] = now; }
    bool pressed = _wasPressed[i];
    if ((uint32_t)(now - _rawSince[i]) >= DEBOUNCE_MS) pressed = _rawPressed[i];

    InputEvent ev = mapDirection(_map, it.dir);
    if (pressed && !_wasPressed[i]) {              // press edge: fire immediately, arm repeat
      _wasPressed[i] = true;
      _nextRepeat[i] = now + REPEAT_DELAY_MS;
      if (ev != InputEvent::None) { out.event = ev; out.ch = 0; return true; }
    } else if (pressed) {                          // held: Up/Down auto-repeat (fast scroll)
      bool repeats = (it.dir == Direction::Up || it.dir == Direction::Down);
      if (repeats && (int32_t)(now - _nextRepeat[i]) >= 0) {
        _nextRepeat[i] = now + REPEAT_INTERVAL_MS;
        if (ev != InputEvent::None) { out.event = ev; out.ch = 0; return true; }
      }
    } else {
      _wasPressed[i] = false;                      // released
    }
  }
  return false;
}

}  // namespace mishmesh
