#pragma once

#include <stdint.h>
#include <mishmesh/core/Applet.h>       // AppletContext, AppServices
#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

class Canvas;

// The Home quick-toggles shade: BLE / Sound / GPS / Advert / Theme tiles.
// Owned and drawn by HomeApplet (not pushed onto the applet stack) so the
// real face stays visible under the panel while it slides. While open it
// consumes all input; Back or NavUp starts the slide-out.
//
// Tile language: FILL is state (filled = on/audible, outline = off or a
// stateless action), the RING around a tile is focus. The BLE tile is
// omitted entirely on builds without BLE (bleSupported() false).
class QuickDrawer {
public:
  enum Tile : int { Ble = 0, Sound, Gps, Advert, Theme, TILE_COUNT };

  void bind(const AppletContext& ctx);
  void open();
  void closeNow();                 // snap shut, e.g. Home losing foreground
  bool isOpen() const { return _open || _h > 0; }
  bool animating() const { return _h != (_open ? PANEL_H : 0); }
  int  selected() const { return _sel; }                    // visible-row index
  int  selectedTile() const { return _tiles[_sel]; }        // Tile id at the selection
  int  tileCount() const { return _ntiles; }

  void draw(Canvas& c);            // full-screen overlay
  bool onInput(InputEvent ev);

private:
  static const int PANEL_H = 38;   // focus ring + tiles row + state line + edge

  void rebuildTiles();             // visible tiles for this build/app
  void activate(bool longPress);
  uint16_t tileIcon(int t) const;
  bool tileOn(int t) const;        // filled (on) vs outline (off/action)
  void stateLabel(int t, char* out, int cap) const;

  AppServices* _app = nullptr;
  sound::SoundEngine* _sound = nullptr;
  AppletHost* _host = nullptr;
  bool _open = false;
  int  _h = 0;                     // current panel height (animated)
  int  _sel = 0;
  uint8_t _tiles[TILE_COUNT] = {0};
  int  _ntiles = 0;
};

}  // namespace mishmesh
