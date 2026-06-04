#include <mishmesh/applets/FormApplet.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

void FormApplet::configure(const char* title, const Field* fields, int n,
                           FormSubmitFn onSubmit, void* ctx) {
  _title = title ? title : "";
  if (n > MAX_FIELDS) n = MAX_FIELDS;
  _n = n;
  for (int i = 0; i < n; i++) _fields[i] = fields[i];
  _onSubmit = onSubmit;
  _ctx = ctx;
}

void FormApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _list.setModel(&_model);
  _list.setRowHeight(14);
  _list.resetSelection();
}

int FormApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int barH = 13;
  c.drawText(fontBody(), 2, 3, _title, DisplayDriver::LIGHT);
  int bodyY = barH + 1;
  _list.draw(c, 0, bodyY, w, h - bodyY);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool FormApplet::submit() {
  for (int i = 0; i < _n; i++) {
    const char* s = _fields[i].buf;
    bool ok = _fields[i].validate ? _fields[i].validate(s) : (s && s[0] != 0);
    if (!ok) {
      _list.setSelected(i);
      if (_host) _host->postToast(_fields[i].errMsg ? _fields[i].errMsg : "Required");
      return false;
    }
  }
  return _onSubmit ? _onSubmit(_ctx) : true;
}

bool FormApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;        // Nav handled by the list
  if (ev == InputEvent::Select) {
    int row = _list.selected();
    if (row < _n) {
      keypadApplet().configure(_fields[row].buf, _fields[row].cap,
                               _fields[row].label, &FormApplet::noopConfirm, nullptr);
      if (_host) _host->push(&keypadApplet());
    } else if (submit() && _host) {
      _host->pop();
    }
    return true;
  }
  return false;   // Back/Cancel bubble -> host pops (cancel)
}

FormApplet& formApplet() {
  static FormApplet a;
  return a;
}

}  // namespace mishmesh
