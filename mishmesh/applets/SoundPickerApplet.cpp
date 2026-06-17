#include <mishmesh/applets/SoundPickerApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

using sound::notifyToneCount;
using sound::notifyToneName;
using sound::notifyTypeDefault;
using sound::SoundId;

SoundPickerApplet::SoundPickerApplet() : Applet("Sound") {}

void SoundPickerApplet::setGlobal(bool channel, const char* title) {
  _perChat = false; _channel = channel;
  strncpy(_title, title ? title : "Sound", sizeof(_title) - 1);
  _title[sizeof(_title) - 1] = 0;
}

void SoundPickerApplet::setChat(const ConvoKey& key, const char* title) {
  _perChat = true; _key = key; _channel = (key.type == 1);
  strncpy(_title, title ? title : "Sound", sizeof(_title) - 1);
  _title[sizeof(_title) - 1] = 0;
}

uint8_t SoundPickerApplet::currentByte() const {
  if (_perChat) return _svc ? _svc->chatSound(_key) : 0;
  return _app ? _app->notifyTone(_channel) : 0;
}

// Row layout. Per-chat: 0=Default, 1..N=tones, N+1=Silent.
// Global:           0..N-1=tones, N=Silent.
int SoundPickerApplet::count() const {
  return notifyToneCount() + (_perChat ? 2 : 1);
}

uint8_t SoundPickerApplet::encodedForRow(int row) const {
  int n = notifyToneCount();
  if (_perChat) {
    if (row == 0) return sound::NOTIFY_TONE_DEFAULT;
    if (row >= 1 && row <= n) return (uint8_t)(sound::NOTIFY_TONE_BASE + (row - 1));
    return sound::NOTIFY_TONE_SILENT;
  }
  if (row >= 0 && row < n) return (uint8_t)(sound::NOTIFY_TONE_BASE + row);
  return sound::NOTIFY_TONE_SILENT;
}

int SoundPickerApplet::rowForEncoded(uint8_t e) const {
  int n = notifyToneCount();
  if (e == sound::NOTIFY_TONE_SILENT) return count() - 1;
  if (e == sound::NOTIFY_TONE_DEFAULT) {
    if (_perChat) return 0;                              // per-chat: the "Default" row
    sound::SoundId def = sound::notifyTypeDefault(_channel);  // global: highlight the firmware default tone
    for (int i = 0; i < n; i++) if (sound::notifyToneId(i) == def) return i;
    return 0;
  }
  int i = (int)e - sound::NOTIFY_TONE_BASE;
  if (i < 0 || i >= n) return 0;
  return _perChat ? i + 1 : i;
}

const char* SoundPickerApplet::label(int i) const {
  uint8_t e = encodedForRow(i);
  if (e == sound::NOTIFY_TONE_DEFAULT && _perChat) return "Default";
  if (e == sound::NOTIFY_TONE_SILENT) return "Silent";
  return notifyToneName((int)e - sound::NOTIFY_TONE_BASE);
}

bool SoundPickerApplet::radioOn(int i) const {
  return rowForEncoded(currentByte()) == i;
}

void SoundPickerApplet::onStart(AppletContext& ctx) {
  _app = ctx.app; _svc = ctx.messages; _snd = ctx.sound;
  _list.setRowHeight(12);
  _list.setModel(this);
  _list.resetSelection();
  _list.setSelected(rowForEncoded(currentByte()));   // open on the active choice, not row 0
}

int SoundPickerApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  const int barH = c.fontHeight(fontBody()) + 3;
  _bar.setTitle(_title);
  if (_app) _bar.setBattery(_app->batteryMillivolts());
  _bar.draw(c, 0, 0, w, barH);
  _list.draw(c, 0, barH, w, h - barH);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool SoundPickerApplet::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    uint8_t e = encodedForRow(_list.selected());
    if (_perChat) { if (_svc) _svc->setChatSound(_key, e); }
    else          { if (_app) _app->setNotifyTone(_channel, e); }
    // Preview: play the resolved tone unless Silent. For per-chat Default, use the
    // real per-type default so the preview matches what will actually play.
    if (_snd) {
      uint8_t typeByte = _app ? _app->notifyTone(_channel) : 0;
      SoundId id;
      bool audible = sound::resolveNotifyTone(_perChat ? e : 0,
                                              _perChat ? typeByte : e,
                                              notifyTypeDefault(_channel), id);
      if (audible) _snd->play(id);
    }
    return true;   // radio mark refreshes from currentByte() on next render
  }
  return false;    // Back bubbles -> host pops to the caller
}

SoundPickerApplet& soundPickerApplet() {
  static SoundPickerApplet s;
  return s;
}

}  // namespace mishmesh
