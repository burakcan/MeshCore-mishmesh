#include <mishmesh/applets/ContactsFullApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const uint32_t DISMISS_MS = 6000;   // auto-close; informational, not an alarm

void ContactsFullApplet::onStart(AppletContext& ctx) {
  _host  = ctx.host;
  _sound = ctx.sound;
}

void ContactsFullApplet::dismiss() {
  if (_sound) _sound->stop();
  if (_host && _host->foreground() == this) _host->pop();
}

int ContactsFullApplet::onRender(Canvas& c) {
  uint32_t now = c.now();
  if (_raisedAt == 0) _raisedAt = now;
  if (now - _raisedAt >= DISMISS_MS) { dismiss(); return 0; }
  if (_sound && !_beeped) {           // one soft attention tone, follows system volume
    _beeped = true;
    _sound->play(sound::SoundId::UiError);
  }

  int w = c.width(), h = c.height();
  c.drawGlyph(iconFont(), w / 2 - 6, 4, (uint16_t)Icon::Warning, DisplayDriver::LIGHT);
  c.drawText(fontSubtitle(), w / 2, 19, "Contacts full", DisplayDriver::LIGHT, TextAlign::Center);

  char buf[12];
  snprintf(buf, sizeof(buf), "%u/%u", (unsigned)_used, (unsigned)_max);
  c.drawText(fontNum(), w / 2, 33, buf, DisplayDriver::LIGHT, TextAlign::Center);

  c.drawText(fontCaption(), w / 2, h - 7, "Press any key to dismiss",
             DisplayDriver::LIGHT, TextAlign::Center);
  return 250;
}

bool ContactsFullApplet::onInput(InputEvent) {
  dismiss();
  return true;   // swallow everything, including Back
}

ContactsFullApplet& contactsFullApplet() {
  static ContactsFullApplet a;
  return a;
}

}  // namespace mishmesh
