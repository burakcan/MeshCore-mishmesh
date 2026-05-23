#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/InputSource.h>
#include <helpers/ui/DisplayDriver.h>

namespace mishmesh {

AppletHost::AppletHost(DisplayDriver* display, const AppletContext& ctx)
    : _display(display), _canvas(display), _ctx(ctx),
      _depth(0), _nsources(0),
      _next_render_at(0), _has_rendered(false), _dirty(true) {
  for (int i = 0; i < MAX_STACK; i++) _stack[i] = nullptr;
  for (int i = 0; i < MAX_SOURCES; i++) _sources[i] = nullptr;
  _ctx.host = this;
}

void AppletHost::addSource(InputSource* src) {
  if (src == nullptr || _nsources >= MAX_SOURCES) return;
  _sources[_nsources++] = src;
}

Applet* AppletHost::foreground() const {
  return _depth > 0 ? _stack[_depth - 1] : nullptr;
}

void AppletHost::setRoot(Applet* root) {
  if (root == nullptr || _depth != 0) return;
  _stack[0] = root;
  _depth = 1;
  root->onStart(_ctx);
  root->onForeground();
  _dirty = true;
}

void AppletHost::push(Applet* a) {
  if (a == nullptr || _depth >= MAX_STACK) return;
  Applet* cur = foreground();
  if (cur != nullptr) cur->onBackground();
  _stack[_depth++] = a;
  a->onStart(_ctx);
  a->onForeground();
  _dirty = true;
}

void AppletHost::pop() {
  if (_depth <= 1) return;   // keep the root
  Applet* top = _stack[_depth - 1];
  top->onBackground();
  top->onStop();
  _stack[--_depth] = nullptr;
  Applet* revealed = foreground();
  if (revealed != nullptr) revealed->onForeground();
  _dirty = true;
}

void AppletHost::dispatch(InputEvent ev) {
  Applet* fg = foreground();
  if (fg == nullptr) return;
  bool consumed = fg->onInput(ev);
  if (!consumed && ev == InputEvent::Back) {
    pop();
  }
  _dirty = true;
}

void AppletHost::loop(uint32_t now_ms) {
  for (int i = 0; i < _nsources; i++) {
    InputReport rep;
    while (_sources[i] != nullptr && _sources[i]->poll(rep)) {
      dispatch(rep.event);
    }
  }
  renderIfDue(now_ms);
}

void AppletHost::renderIfDue(uint32_t now_ms) {
  Applet* fg = foreground();
  if (fg == nullptr || _display == nullptr) return;

  bool due = _dirty || !_has_rendered || now_ms >= _next_render_at;
  if (!due) return;

  _canvas.setNow(now_ms);
  _display->startFrame();
  int delay = fg->onRender(_canvas);
  _display->endFrame();

  if (delay < 0) delay = 0;
  _next_render_at = now_ms + (uint32_t)delay;
  _has_rendered = true;
  _dirty = false;
}

}  // namespace mishmesh
