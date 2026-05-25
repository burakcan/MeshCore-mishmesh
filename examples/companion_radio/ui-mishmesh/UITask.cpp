#include "UITask.h"
#include "target.h"   // rtc_clock

uint32_t UITask::epochSeconds() const {
  return rtc_clock.getCurrentTime();
}

uint16_t UITask::batteryMillivolts() const {
  uint32_t now = millis();
  if (_batt_mv == 0 || now - _batt_sampled_at >= 8000) {
    uint16_t mv = getBattMilliVolts();
    _batt_mv = _batt_mv ? (uint16_t)((_batt_mv * 3 + mv) / 4) : mv;
    _batt_sampled_at = now;
  }
  return _batt_mv;
}

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _node_prefs = node_prefs;

  if (_display == nullptr) return;   // headless build

  mishmesh::AppletContext ctx;
  ctx.app = this;
  _host = new mishmesh::AppletHost(_display, ctx);

  _menu = new mishmesh::AppMenuApplet();
  _home = new mishmesh::HomeApplet();
  _home->setMenu(_menu);
  _host->setRoot(_home);

#ifdef UI_HAS_JOYSTICK
  // Wio Tracker L1 buttons are active-low (pull-up), hence reverse=true.
  mishmesh::DirectionalMap dirMap;
  _joystick = new mishmesh::DirectionalSource(
      JOYSTICK_UP, JOYSTICK_DOWN, JOYSTICK_LEFT, JOYSTICK_RIGHT, JOYSTICK_PRESS, dirMap, 1000, /*reverse*/true);
  _joystick->begin();
  _host->addSource(_joystick);

  mishmesh::GestureMap backMap;
  backMap.click = mishmesh::InputEvent::Back;
  backMap.longPress = mishmesh::InputEvent::BackLong;
  _backBtn = new mishmesh::ButtonGestureSource(PIN_BACK_BTN, backMap, 1000, /*reverse*/true);
  _backBtn->begin();
  _host->addSource(_backBtn);
#else
  mishmesh::GestureMap userMap;
  userMap.click = mishmesh::InputEvent::NavDown;
  userMap.doubleClick = mishmesh::InputEvent::Select;
  userMap.longPress = mishmesh::InputEvent::Back;
  _userBtn = new mishmesh::ButtonGestureSource(PIN_USER_BTN, userMap, 1000, /*reverse*/true);
  _userBtn->begin();
  _host->addSource(_userBtn);
#endif
}

void UITask::msgRead(int /*msgcount*/) {
}

void UITask::newMsg(uint8_t /*path_len*/, const char* /*from_name*/,
                    const char* /*text*/, int /*msgcount*/) {
}

void UITask::notify(UIEventType /*t*/) {
}

void UITask::loop() {
  if (_host != nullptr) {
    _host->loop(millis());
  }
}
