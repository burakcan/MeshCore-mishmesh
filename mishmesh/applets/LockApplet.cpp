#include <mishmesh/applets/LockApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>
#include <helpers/ui/DisplayDriver.h>
#include <math.h>

namespace mishmesh {

// A fat pixel: draws a filled square so the shackle stroke is a few px wide and
// stays connected after rotation (axis-aligned rects can't slant).
static void fatDot(Canvas& c, float x, float y, DisplayDriver::Color col) {
  c.fillRect((int)lroundf(x) - 1, (int)lroundf(y) - 1, 3, 3, col);
}

void LockApplet::onStart(AppletContext& ctx) {
  _host  = ctx.host;
  _sound = ctx.sound;
}

void LockApplet::onForeground() {
  // A fresh push (from home) plays the "Locked" flourish; a re-foreground after
  // some overlay above us was dismissed just returns to the resting lock.
  _mode = Locking;
  _pips = 0;
  _flash = 0;
  _animAt = _host ? _host->nowMs() : 0;
  if (_sound) _sound->play(sound::SoundId::UiConfirm);
}

bool LockApplet::onInput(InputEvent ev) {
  const uint32_t now = _host ? _host->nowMs() : 0;
  const bool isBack = (ev == InputEvent::Back || ev == InputEvent::BackLong);

  if (_mode == Unlocking) return true;   // swallow input mid-flourish

  if (_mode == Locking) {                // a press cuts the intro short -> challenge
    _mode = Challenge;
    _pips = isBack ? 1 : 0;
    _flash = isBack ? 3 : 0;
    _lastMs = now;
    if (isBack && _sound) _sound->play(sound::SoundId::UiTick);
    return true;
  }

  if (_mode == Resting) {
    // Any key surfaces the challenge. A Back that reveals it also counts as the
    // first unlock press, so three Backs from the locked home unlock directly.
    _mode = Challenge;
    _pips = isBack ? 1 : 0;
    _flash = isBack ? 3 : 0;
    _lastMs = now;
    if (isBack && _sound) _sound->play(sound::SoundId::UiTick);
    return true;
  }

  // Challenge: Back fills a pip; anything else just keeps the challenge alive.
  _lastMs = now;
  if (isBack) {
    if (_pips < 3) _pips++;
    _flash = 3;
    if (_sound) _sound->play(sound::SoundId::UiTick);
    if (_pips >= 3) {
      _mode = Unlocking;
      _animAt = now;
      if (_sound) _sound->play(sound::SoundId::UiConfirm);
    }
  }
  return true;   // gate everything; nothing reaches the home screen beneath
}

// Padlock from primitives: a rounded body with a keyhole, and a shackle drawn as
// a stroked arch + two legs. The left leg stays anchored in the body; the arch
// and right leg pivot about the left leg's top so open01 swings them up and open.
void LockApplet::drawPadlock(Canvas& c, int cx, int bodyTop, float open01) {
  const float R = 6.0f;           // shackle radius (leg half-spacing)
  const int   legLen = 6;         // straight run below the arch
  const int   archCenterY = bodyTop - 2;
  const int   legBottom = bodyTop + 4;      // legs sink into the body
  const int   lx = cx - (int)R;             // left leg x (fixed)
  const int   rx = cx + (int)R;             // right leg x (pivots)

  // Fixed left leg.
  for (int y = archCenterY; y <= legBottom; y++) fatDot(c, lx, y, DisplayDriver::LIGHT);

  // Arch + right leg pivot about the left leg's top.
  const float px = (float)lx, py = (float)archCenterY;
  const float ang = -open01 * 1.9f;         // swing open (up and over the pivot)
  const float s = sinf(ang), co = cosf(ang);
  auto plot = [&](float x, float y) {
    const float dx = x - px, dy = y - py;
    fatDot(c, px + dx * co - dy * s, py + dx * s + dy * co, DisplayDriver::LIGHT);
  };
  // arch: half circle over the top, from the left leg round to the right leg
  for (int i = 0; i <= 20; i++) {
    const float th = (float)M_PI * (1.0f - i / 20.0f);   // pi -> 0
    plot(cx + R * cosf(th), archCenterY - R * sinf(th));
  }
  // right leg
  for (int y = archCenterY; y <= legBottom; y++) plot((float)rx, (float)y);

  // Body over the leg bottoms, then the keyhole punched out of it.
  const int bw = 26, bh = 20, bx = cx - bw / 2;
  c.fillRoundRect(bx, bodyTop, bw, bh, DisplayDriver::LIGHT);
  c.fillRoundRect(cx - 2, bodyTop + 6, 4, 4, DisplayDriver::DARK);
  c.fillRect(cx - 1, bodyTop + 10, 2, 4, DisplayDriver::DARK);
}

// Clamp then ease-out (decelerating) - used to shape both flourishes.
static float easeOut(float p) {
  if (p < 0) p = 0; else if (p > 1) p = 1;
  return 1.0f - (1.0f - p) * (1.0f - p);
}

int LockApplet::onRender(Canvas& c) {
  if (_mode == Resting) return 1000;     // draw nothing; home shows through

  const uint32_t now = c.now();

  if (_mode == Challenge && now - _lastMs > CHALLENGE_TIMEOUT_MS) {
    _mode = Resting; _pips = 0; _flash = 0;
    return 1000;
  }
  if (_mode == Locking && now - _animAt > LOCK_ANIM_MS) {
    _mode = Resting;
    return 1000;
  }
  if (_mode == Unlocking && now - _animAt > UNLOCK_ANIM_MS) {
    if (_host) _host->pop();             // remove the lock -> home revealed
    return 0;
  }

  const int w = c.width(), h = c.height();
  c.fillRect(0, 0, w, h, DisplayDriver::DARK);   // opaque backdrop over home

  const mf_font_s* f = fontBody();
  const int fh = c.fontHeight(f);
  const int pipH = 8;
  const int textY = h - 2 - fh;             // hint sits just off the bottom edge
  const int pipY  = textY - 5 - pipH;       // pips sit a clear band above the hint

  const int bodyTop = pipY - 6 - 20;        // padlock (body h=20) clears the pips
  float open01 = 0.0f;
  if (_mode == Unlocking) open01 = easeOut((float)(now - _animAt) / UNLOCK_ANIM_MS);
  else if (_mode == Locking) open01 = 1.0f - easeOut((float)(now - _animAt) / LOCK_ANIM_MS);
  drawPadlock(c, w / 2, bodyTop, open01);

  if (_mode == Challenge || _mode == Unlocking) {
    const int pipW = 8, gap = 7;
    const int totalW = 3 * pipW + 2 * gap;
    const int px = (w - totalW) / 2;
    for (int i = 0; i < 3; i++) {
      const int x = px + i * (pipW + gap);
      const bool filled = (_mode == Unlocking) || i < _pips;
      if (filled) c.fillRoundRect(x, pipY, pipW, pipH, DisplayDriver::LIGHT);
      else        c.drawRoundRect(x, pipY, pipW, pipH, DisplayDriver::LIGHT);
      if (_flash && filled && i == _pips - 1)
        c.drawRect(x - 2, pipY - 2, pipW + 4, pipH + 4, DisplayDriver::LIGHT);
    }
  }

  const char* hint = (_mode == Unlocking) ? "Unlocked"
                   : (_mode == Locking)   ? "Locked"
                                          : "Back x3 to unlock";
  c.drawText(f, w / 2, textY, hint, DisplayDriver::LIGHT, TextAlign::Center);

  if (_flash) _flash--;
  if (_mode == Locking || _mode == Unlocking || _flash) return 33;   // animate
  return 400;                                                         // idle-timeout wake
}

}  // namespace mishmesh
