#include <mishmesh/applets/settings/RepeatAlertPanel.h>
#include <mishmesh/core/Canvas.h>
#include <stdio.h>

namespace mishmesh {

static const uint8_t INTERVAL_OPTS[] = { 0, 1, 2, 5, 10, 15, 30 };
static const uint8_t STOP_OPTS[]     = { 5, 15, 30, 60, 0 };   // 0 = never ring out

static int optIndex(const uint8_t* opts, int n, int mins) {
  for (int i = 0; i < n; i++) if (opts[i] == mins) return i;
  return 0;
}

void repeatIntervalLabel(int mins, char* out, uint16_t cap) {
  if (mins <= 0) snprintf(out, cap, "Off");
  else           snprintf(out, cap, "%d min", mins);
}

void repeatStopLabel(int mins, char* out, uint16_t cap) {
  if (mins <= 0)      snprintf(out, cap, "Never");
  else if (mins == 60) snprintf(out, cap, "1 hour");
  else                 snprintf(out, cap, "%d min", mins);
}

static void intervalStepLabel(int idx, char* out, uint16_t cap) {
  repeatIntervalLabel(INTERVAL_OPTS[idx], out, cap);
}
static void stopStepLabel(int idx, char* out, uint16_t cap) {
  repeatStopLabel(STOP_OPTS[idx], out, cap);
}

const char* RepeatAlertPanel::Model::label(int i) const {
  return i == 0 ? "Every" : "Stop after";
}

const char* RepeatAlertPanel::Model::value(int i) const {
  static char buf[10];
  MessagesConfig c = svc ? svc->getMessagesConfig() : MessagesConfig();
  if (i == 0) repeatIntervalLabel(c.repeatMins, buf, sizeof(buf));
  else        repeatStopLabel(c.repeatStopMins, buf, sizeof(buf));
  return buf;
}

void RepeatAlertPanel::begin(AppletContext& ctx) {
  _svc = ctx.messages;
  _model.svc = _svc;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();   // singleton reuse: setModel skips reset on same-ptr rebind
  _editing = Row::None;
}

int RepeatAlertPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  if (_editing != Row::None) { _stepper.draw(c, 0, 0, c.width(), c.height()); return 100; }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool RepeatAlertPanel::onInput(InputEvent ev) {
  if (_editing != Row::None) {
    if (_stepper.onInput(ev)) {
      StepperResult r = _stepper.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed && _svc) {
          MessagesConfig cfg = _svc->getMessagesConfig();
          if (_editing == Row::Every) cfg.repeatMins = INTERVAL_OPTS[_stepper.value()];
          else                        cfg.repeatStopMins = STOP_OPTS[_stepper.value()];
          _svc->setMessagesConfig(cfg);
        }
        _editing = Row::None;
        _stepper.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    MessagesConfig cfg = _svc ? _svc->getMessagesConfig() : MessagesConfig();
    if (_list.selected() == (int)Row::Every) {
      _stepper.configure("Every", optIndex(INTERVAL_OPTS, 7, cfg.repeatMins),
                         0, 6, intervalStepLabel);
      _editing = Row::Every;
    } else {
      _stepper.configure("Stop after", optIndex(STOP_OPTS, 5, cfg.repeatStopMins),
                         0, 4, stopStepLabel);
      _editing = Row::StopAfter;
    }
    return true;
  }
  return false;   // Back bubbles
}

RepeatAlertPanel& repeatAlertSettings() {
  static RepeatAlertPanel s;
  return s;
}

}  // namespace mishmesh
