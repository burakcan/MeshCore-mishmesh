// mishmesh/applets/RepeaterManageApplet.cpp
#include <mishmesh/applets/RepeaterManageApplet.h>
#include <mishmesh/applets/CommandLineApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

// Fixed hub rows. Index order is contract (see onInput + tests).
enum { ROW_STATUS = 0, ROW_CLI = 1, ROW_SETTINGS = 2, ROW_COUNT = 3 };

struct HubModel : ListModel {
  int count() const override { return ROW_COUNT; }
  const char* label(int i) const override {
    switch (i) {
      case ROW_STATUS:   return "Status";
      case ROW_CLI:      return "Command line";
      default:           return "Settings";
    }
  }
};
static HubModel s_hubModel;

void RepeaterManageApplet::setTarget(const uint8_t* pubKey, const char* name) {
  memcpy(_pub, pubKey, 6);
  if (name) { strncpy(_name, name, sizeof(_name) - 1); _name[sizeof(_name) - 1] = 0; }
  else _name[0] = 0;
}

void RepeaterManageApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _menu.setModel(&s_hubModel);
  _menu.resetSelection();
}

int RepeaterManageApplet::onRender(Canvas& c) {
  const int w = c.width(), h = c.height();
  const Font* cap = fontCaption();
  int caph = c.lineHeight(cap);
  c.drawText(cap, 2, 1, _name[0] ? _name : "Repeater", DisplayDriver::LIGHT, TextAlign::Left);
  int top = caph + 2;
  _menu.draw(c, 0, top, w, h - top);
  return _menu.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool RepeaterManageApplet::onInput(InputEvent ev) {
  if (_menu.onInput(ev)) return true;      // NavUp/Down move the selection
  if (ev == InputEvent::Select) {
    switch (_menu.selected()) {
      case ROW_CLI:
        commandLineApplet().setTarget(_pub, _name);
        if (_host) _host->push(&commandLineApplet());
        return true;
      default:
        if (_host) _host->postToast("Coming soon");
        return true;
    }
  }
  return false;   // Back bubbles: host pops us back to contact detail
}

RepeaterManageApplet& repeaterManageApplet() {
  static RepeaterManageApplet s_repeaterManage;
  return s_repeaterManage;
}

}  // namespace mishmesh
