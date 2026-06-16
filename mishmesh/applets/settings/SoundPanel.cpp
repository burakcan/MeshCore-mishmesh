#include <mishmesh/applets/settings/SoundPanel.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/sound/Sounds.h>

namespace mishmesh {

using V = sound::VolumeLevel;

const char* SoundPanel::levelName(V v) {
  switch (v) {
    case V::Mute: return "Mute";
    case V::Low:  return "Low";
    case V::Mid:  return "Mid";
    default:      return "High";
  }
}

V SoundPanel::nextLevel(V v) {
  switch (v) {
    case V::Mute: return V::Low;
    case V::Low:  return V::Mid;
    case V::Mid:  return V::High;
    default:      return V::Mute;
  }
}

void SoundPanel::begin(AppletContext& ctx) {
  _snd  = ctx.sound;
  _app  = ctx.app;
  _host = ctx.host;
  _model.snd = _snd;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
}

int SoundPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool SoundPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    if (_snd) {
      V next = nextLevel(_snd->volume());
      // Route through AppServices so the adapter applies AND persists it; fall
      // back to a live-only change when no app seam is present.
      if (_app) _app->setSoundVolume((uint8_t)next); else _snd->setVolume(next);
      if (next != V::Mute) _snd->play(sound::SoundId::UiConfirm);   // preview
      if (_host) _host->postToast(levelName(next));
    }
    return true;
  }
  return false;   // Back bubbles
}

SoundPanel& soundSettings() {
  static SoundPanel s;
  return s;
}

}  // namespace mishmesh
