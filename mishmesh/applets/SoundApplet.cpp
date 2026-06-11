#include <mishmesh/applets/SoundApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

using V = sound::VolumeLevel;

const char* SoundApplet::levelName(V v) {
  switch (v) {
    case V::Mute: return "Mute";
    case V::Low:  return "Low";
    case V::Mid:  return "Mid";
    default:      return "High";
  }
}

V SoundApplet::nextLevel(V v) {
  switch (v) {
    case V::Mute: return V::Low;
    case V::Low:  return V::Mid;
    case V::Mid:  return V::High;
    default:      return V::Mute;
  }
}

void SoundApplet::onStart(AppletContext& ctx) {
  _snd  = ctx.sound;
  _app  = ctx.app;
  _host = ctx.host;
}

int SoundApplet::onRender(Canvas& c) {
  const int W = c.width();
  const int cx = W / 2;
  const DisplayDriver::Color fg = DisplayDriver::LIGHT;
  V level = _snd ? _snd->volume() : V::Mute;

  c.drawGlyph(iconFont(), cx - 6, 4, (uint16_t)Icon::Bell, fg);
  c.drawText(fontBody(), cx, 22, levelName(level), fg, TextAlign::Center);
  c.drawText(fontBody(), cx, 44, "Select: change", fg, TextAlign::Center);
  return 0;   // static screen; repaint on input
}

bool SoundApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Select) {
    if (_snd) {
      V next = nextLevel(_snd->volume());
      // Route through AppServices so the adapter applies it AND persists it to
      // NodePrefs; fall back to a live-only change if no app seam is present.
      if (_app) _app->setSoundVolume((uint8_t)next); else _snd->setVolume(next);
      if (next != V::Mute) _snd->play(sound::SoundId::UiConfirm);   // preview at the new level
      if (_host) _host->postToast(levelName(next));
    }
    return true;
  }
  return false;   // Back (and everything else) bubbles to the host
}

static SoundApplet s_sound;
MISHMESH_REGISTER_APPLET_ICON(&s_sound, ::mishmesh::Placement::AppMenu,
                              "Sound", 8, (uint16_t)::mishmesh::Icon::Bell);

}  // namespace mishmesh
