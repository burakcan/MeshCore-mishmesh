#include "UITask.h"

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _node_prefs = node_prefs;

  if (_display == nullptr) return;   // headless build

  mishmesh::AppletContext ctx;
  _host = new mishmesh::AppletHost(_display, ctx);

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

  // No applets registered yet: blank the screen so the boot text doesn't linger.
  _display->startFrame();
  _display->endFrame();
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
