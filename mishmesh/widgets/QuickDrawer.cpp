#include <mishmesh/widgets/QuickDrawer.h>
#include <mishmesh/core/Actions.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {

static const int TILE = 18;      // tile box, 12px glyph + 3px padding
static const int GAP = 4;        // leaves 2px for the focus ring between tiles
static const int ICON_PAD = 3;

void QuickDrawer::bind(const AppletContext& ctx) {
  _app = ctx.app;
  _sound = ctx.sound;
  _host = ctx.host;
}

void QuickDrawer::rebuildTiles() {
  _ntiles = 0;
  // A null app (host tests) keeps every tile so the fixture stays simple.
  if (!_app || _app->bleSupported()) _tiles[_ntiles++] = Ble;
  _tiles[_ntiles++] = Sound;
  _tiles[_ntiles++] = Gps;
  _tiles[_ntiles++] = Advert;
  _tiles[_ntiles++] = Theme;
}

void QuickDrawer::open() {
  rebuildTiles();
  _open = true;
  _sel = 0;
}

void QuickDrawer::closeNow() {
  _open = false;
  _h = 0;
}

uint16_t QuickDrawer::tileIcon(int t) const {
  switch (t) {
    case Ble:    return (uint16_t)Icon::Bluetooth;
    case Sound:  return (uint16_t)((_sound && _sound->volume() == sound::VolumeLevel::Mute)
                                       ? Icon::VolumeMute : Icon::Volume);
    case Gps:    return (uint16_t)Icon::Gps;
    case Advert: return (uint16_t)Icon::Radio;
    default:     return (uint16_t)(uiPrefs().darkMode() ? Icon::Moon : Icon::Sun);
  }
}

bool QuickDrawer::tileOn(int t) const {
  switch (t) {
    case Ble:    return _app && _app->bleEnabled();
    case Sound:  return _sound && _sound->volume() != sound::VolumeLevel::Mute;
    case Gps:    return _app && _app->gpsEnabled();
    case Theme:  return !uiPrefs().darkMode();   // filled while light mode is active
    default:     return false;                   // Advert is a stateless action
  }
}

void QuickDrawer::stateLabel(int t, char* out, int cap) const {
  switch (t) {
    case Ble: {
      bool en = _app && _app->bleEnabled();
      if (!en) snprintf(out, cap, "Bluetooth - off");
      else if (_app->bleConnected()) snprintf(out, cap, "Bluetooth - connected");
      else if (_app->blePin())   // waiting for a client: show what pairing asks for
        snprintf(out, cap, "BT PIN %u", (unsigned)_app->blePin());
      else snprintf(out, cap, "Bluetooth - on");
      break;
    }
    case Sound: {
      uint8_t v = _sound ? (uint8_t)_sound->volume() : 2;
      snprintf(out, cap, "Sound - %s", volumeLevelName(v));
      break;
    }
    case Gps:
      if (_app && !_app->gpsSupported()) snprintf(out, cap, "GPS - none");
      else if (!(_app && _app->gpsEnabled())) snprintf(out, cap, "GPS - off");
      else if (!_app->gpsHasFix()) snprintf(out, cap, "GPS - searching");
      else if (_app->gpsSatellites() > 0)
        snprintf(out, cap, "GPS - %d sats", _app->gpsSatellites());
      else snprintf(out, cap, "GPS - fix");
      break;
    case Advert:
      snprintf(out, cap, "Advert - hold: flood");
      break;
    default:
      snprintf(out, cap, "Theme - %s", uiPrefs().darkMode() ? "dark" : "light");
      break;
  }
}

void QuickDrawer::activate(bool longPress) {
  switch (_tiles[_sel]) {
    case Ble:
      toggleBle(_app, _host);   // same path as the Bluetooth settings panel
      break;
    case Sound:
      cycleSoundVolume(_app, _sound, _host);   // same path as the Sound settings panel
      break;
    case Gps:
      if (!_app) break;
      if (!_app->gpsSupported()) { if (_host) _host->postToast("No GPS"); break; }
      _app->setGpsEnabled(!_app->gpsEnabled());
      break;
    case Advert: {
      if (!_app) break;
      bool ok = _app->sendAdvert(longPress);
      if (_host)
        _host->postToast(!ok ? "Advert failed"
                             : longPress ? "Flood advert sent" : "Zero-hop advert sent");
      break;
    }
    default:
      // Theme: the whole panel inverting is its own feedback - no toast.
      uiPrefs().setDarkMode(!uiPrefs().darkMode());
      break;
  }
}

bool QuickDrawer::onInput(InputEvent ev) {
  if (!isOpen()) return false;
  switch (ev) {
    case InputEvent::NavLeft:
      _sel = (_sel + _ntiles - 1) % _ntiles;
      return true;
    case InputEvent::NavRight:
      _sel = (_sel + 1) % _ntiles;
      return true;
    case InputEvent::Select:
      activate(false);
      return true;
    case InputEvent::SelectLong:
      activate(true);
      return true;
    case InputEvent::Back:
    case InputEvent::BackLong:
    case InputEvent::NavUp:
      _open = false;
      return true;
    default:
      return true;   // swallow everything while open (incl. NavDown)
  }
}

void QuickDrawer::draw(Canvas& c) {
  _h = approach(_h, _open ? PANEL_H : 0, 8);
  if (_h <= 0) return;
  int w = c.width();
  int top = _h - PANEL_H;          // slides down: content anchored to panel top

  c.fillRect(0, 0, w, _h, DisplayDriver::DARK);            // blank behind the shade
  c.fillRect(0, _h - 1, w, 1, DisplayDriver::LIGHT);       // shade bottom edge

  int rowW = _ntiles * TILE + (_ntiles - 1) * GAP;
  int x0 = (w - rowW) / 2;
  for (int i = 0; i < _ntiles; i++) {
    int t = _tiles[i];
    int tx = x0 + i * (TILE + GAP);
    int ty = top + 4;
    // Fill is state: on = inverted tile, off/action = outline. Focus is the ring.
    if (tileOn(t)) {
      c.fillRect(tx, ty, TILE, TILE, DisplayDriver::LIGHT);
      c.drawGlyph(iconFont(), tx + ICON_PAD, ty + ICON_PAD,
                  tileIcon(t), DisplayDriver::DARK);
    } else {
      c.drawRoundRect(tx, ty, TILE, TILE, DisplayDriver::LIGHT);
      c.drawGlyph(iconFont(), tx + ICON_PAD, ty + ICON_PAD,
                  tileIcon(t), DisplayDriver::LIGHT);
    }
    if (i == _sel)
      c.drawRoundRect(tx - 2, ty - 2, TILE + 4, TILE + 4, DisplayDriver::LIGHT);
  }

  char st[28];
  stateLabel(_tiles[_sel], st, sizeof(st));
  c.drawText(fontBody(), w / 2, top + 4 + TILE + 4, st,
             DisplayDriver::LIGHT, TextAlign::Center);
}

}  // namespace mishmesh
