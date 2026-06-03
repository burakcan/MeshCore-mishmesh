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
  // Level-driven and edge-triggered: the click fires once on the press edge, then
  // (if hold-repeat is on) auto-repeats while held. A press carried over a
  // foreground change is swallowed until released (_suppress).
  uint32_t now = millis();
  bool raw = _btn.isPressed();
  if (raw != _rawPressed) { _rawPressed = raw; _rawSince = now; }
  bool pressed = _wasPressed;
  if ((uint32_t)(now - _rawSince) >= DEBOUNCE_MS) pressed = _rawPressed;

  if (!pressed) {                                  // released: re-arm
    _wasPressed = false;
    _suppress = false;
    return false;
  }
  if (_map.click == InputEvent::None) { _wasPressed = true; return false; }

  if (!_wasPressed) {                              // press edge
    _wasPressed = true;
    _nextRepeat = now + REPEAT_DELAY_MS;
    if (_suppress) return false;                   // held across a context change: ignore
    out.event = _map.click; out.ch = 0; out.repeat = false;
    return true;
  }
  if (_suppress) return false;
  if (_holdRepeat && (int32_t)(now - _nextRepeat) >= 0) {   // held: repeat
    _nextRepeat = now + REPEAT_INTERVAL_MS;
    out.event = _map.click; out.ch = 0; out.repeat = true;
    return true;
  }
  return false;
}

bool DirectionalSource::poll(InputReport& out) {
  // The center press uses the gesture FSM so a hold can be distinguished from a
  // tap (click -> Select, long-press -> SelectLong). check() must run every poll
  // to advance that FSM; the 4 directions stay level-driven for snappy repeats.
  Gesture pg = toGesture(_press.check());

  struct Item { MomentaryButton* btn; Direction dir; };
  Item items[] = {
    {&_up, Direction::Up},
    {&_down, Direction::Down},
    {&_left, Direction::Left},
    {&_right, Direction::Right},
  };
  uint32_t now = millis();
  for (int i = 0; i < 4; i++) {
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
  if (pg == Gesture::Click && _map.press != InputEvent::None) {
    out.event = _map.press; out.ch = 0; return true;
  }
  if (pg == Gesture::LongPress && _map.pressLong != InputEvent::None) {
    out.event = _map.pressLong; out.ch = 0; return true;
  }
  return false;
}

}  // namespace mishmesh
