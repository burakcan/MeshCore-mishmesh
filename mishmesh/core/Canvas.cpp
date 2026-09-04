#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/UiPrefs.h>
#include <mcufont.h>
#include <math.h>
#include <string.h>

namespace mishmesh {

// glyph-overlay hook state (set via Canvas::setEmojiRenderer). Null
// until an overlay registers, so unregistered rendering/measuring is unchanged.
static const mf_font_s* s_emojiFont = nullptr;
static Canvas::EmojiLookupFn s_emojiLookup = nullptr;
static Canvas::EmojiZeroWidthFn s_emojiZeroWidth = nullptr;
// Side bearing (px) added on each side of an overlay glyph so emoji don't butt
// against neighbouring glyphs. Applied identically in mm_char and textWidth.
static constexpr int kEmojiPadPx = 1;

void Canvas::setEmojiRenderer(const mf_font_s* font, EmojiLookupFn lookup,
                              EmojiZeroWidthFn zeroWidth) {
  s_emojiFont = font; s_emojiLookup = lookup; s_emojiZeroWidth = zeroWidth;
}

DisplayDriver::Color themeSwapped(DisplayDriver::Color c) {
  if (uiPrefs().darkMode()) return c;
  return c == DisplayDriver::DARK ? DisplayDriver::LIGHT : DisplayDriver::DARK;
}

static bool& lightBackgroundFlag() { static bool v = false; return v; }
bool lightBackgroundPanel() { return lightBackgroundFlag(); }
void setLightBackgroundPanel(bool on) { lightBackgroundFlag() = on; }

ColorVal themedColor(DisplayDriver::Color c) {
  DisplayDriver::Color s = themeSwapped(c);
  // window_bkg is the dark end on a mono OLED and the light end on e-ink, so on
  // e-ink the two semantic colors have to trade places or the whole theme comes
  // out inverted - dark mode painting white and light mode painting black.
  if (lightBackgroundPanel())
    s = (s == DisplayDriver::DARK) ? DisplayDriver::LIGHT : DisplayDriver::DARK;
  return s == DisplayDriver::DARK ? UIColor::window_bkg : UIColor::primary_txt;
}

// Intersect a local rect with the clip window [cl,cr) x [ct,cb); false if
// nothing remains visible.
static bool clipLocal(int& x, int& y, int& w, int& h,
                      int cl, int ct, int cr, int cb) {
  if (w <= 0 || h <= 0) return false;
  int x2 = x + w;
  int y2 = y + h;
  if (x < cl) x = cl;
  if (y < ct) y = ct;
  if (x2 > cr) x2 = cr;
  if (y2 > cb) y2 = cb;
  w = x2 - x;
  h = y2 - y;
  return w > 0 && h > 0;
}

Canvas Canvas::region(int x, int y, int w, int h) const {
  int cw = w;
  int ch = h;
  if (x + cw > _w) cw = _w - x;
  if (y + ch > _h) ch = _h - y;
  if (cw < 0) cw = 0;
  if (ch < 0) ch = 0;
  // Carry the parent's clip into the child's local frame (shift by -x,-y) and
  // intersect with the child's own [0,cw) x [0,ch) bounds, so a negative offset
  // clips the overflow away instead of letting draws escape the parent.
  int cl = _cl - x; if (cl < 0) cl = 0;
  int ct = _ct - y; if (ct < 0) ct = 0;
  int cr = _cr - x; if (cr > cw) cr = cw;
  int cb = _cb - y; if (cb > ch) cb = ch;
  if (cr < cl) cr = cl;
  if (cb < ct) cb = ct;
  return Canvas(_d, _ox + x, _oy + y, cw, ch, cl, ct, cr, cb, _now);
}

void Canvas::fillRect(int x, int y, int w, int h, DisplayDriver::Color c) {
  if (!clipLocal(x, y, w, h, _cl, _ct, _cr, _cb)) return;
  if (_d) {
    _d->setColor(themedColor(c));
    _d->fillRect(_ox + x, _oy + y, w, h);
  }
}

void Canvas::drawRect(int x, int y, int w, int h, DisplayDriver::Color c) {
  if (!clipLocal(x, y, w, h, _cl, _ct, _cr, _cb)) return;
  if (_d) {
    _d->setColor(themedColor(c));
    _d->drawRect(_ox + x, _oy + y, w, h);
  }
}

void Canvas::drawRoundRect(int x, int y, int w, int h, DisplayDriver::Color c) {
  if (w < 3 || h < 3) { drawRect(x, y, w, h, c); return; }
  fillRect(x + 1, y,         w - 2, 1, c);   // top
  fillRect(x + 1, y + h - 1, w - 2, 1, c);   // bottom
  fillRect(x,         y + 1, 1, h - 2, c);   // left
  fillRect(x + w - 1, y + 1, 1, h - 2, c);   // right
}

void Canvas::fillRoundRect(int x, int y, int w, int h, DisplayDriver::Color c) {
  if (w < 3 || h < 3) { fillRect(x, y, w, h, c); return; }
  fillRect(x + 1, y,         w - 2, h,     c);   // center block, full height
  fillRect(x,         y + 1, 1,     h - 2, c);   // left edge, corners trimmed
  fillRect(x + w - 1, y + 1, 1,     h - 2, c);   // right edge, corners trimmed
}

void Canvas::text(int x, int y, const char* str, DisplayDriver::Color c) {
  if (!_d || str == nullptr) return;
  if (x < _cl || y < _ct || x >= _cr || y >= _cb) return;
  _d->setColor(themedColor(c));
  _d->setCursor(_ox + x, _oy + y);
  _d->print(str);
}

int Canvas::textWidth(const char* str) const {
  return (_d && str) ? _d->getTextWidth(str) : 0;
}

void Canvas::textRight(int x_right, int y, const char* str, DisplayDriver::Color c) {
  text(x_right - textWidth(str), y, str, c);
}

void Canvas::textCentered(int cx, int y, const char* str, DisplayDriver::Color c) {
  text(cx - textWidth(str) / 2, y, str, c);
}

// mcufont renders via horizontal pixel runs; for bw fonts alpha is 0 or 255.
namespace {

struct TextState {
  Canvas* c;
  const mf_font_s* font;
  DisplayDriver::Color col;
  int16_t x, y;
  int scale;              // integer glyph magnification, see emit()
  int16_t ax, ay;         // anchor the scaling is measured from
};

// Everything mcufont emits goes through here. At scale 1 it is a plain fillRect;
// above that, every coordinate is multiplied out from the anchor - which scales
// the glyph advances as well as the glyph bodies, since mcufont lays the line out
// in unscaled space and we transform the result. Whole multiples of a bitmap glyph
// are exact, so this stays as sharp as the 1:1 case.
void emit(TextState* s, int x, int y, int w, int h) {
  if (s->scale <= 1) { s->c->fillRect(x, y, w, h, s->col); return; }
  const int sx = s->ax + (x - s->ax) * s->scale;
  const int sy = s->ay + (y - s->ay) * s->scale;
  s->c->fillRect(sx, sy, w * s->scale, h * s->scale, s->col);
}

void mm_pixel(int16_t x, int16_t y, uint8_t count, uint8_t alpha, void* state) {
  if (alpha < 128) return;
  TextState* s = (TextState*)state;
  emit(s, x, y, count, 1);
}

uint8_t mm_char(int16_t x, int16_t y, mf_char ch, void* state) {
  TextState* s = (TextState*)state;
  // Glyph overlay: zero-width modifiers (VS16/ZWJ/skin tones) draw
  // nothing; mapped codepoints come from the registered overlay atlas, drawn
  // centered on the body line and advancing by the atlas glyph's width (mcufont
  // advances by our return - mf_justify.c). Unregistered -> unchanged path below.
  if (s_emojiZeroWidth && s_emojiZeroWidth((uint16_t)ch)) return 0;
  if (s_emojiFont && s_emojiLookup) {
    uint16_t glyph;
    if (s_emojiLookup((uint16_t)ch, glyph)) {
      int16_t dy = (int16_t)((s->font->height - s_emojiFont->height) / 2);
      mf_render_character(s_emojiFont, (int16_t)(x + kEmojiPadPx), (int16_t)(y + dy),
                          glyph, mm_pixel, state);
      return (uint8_t)(s_emojiFont->character_width(s_emojiFont, glyph) + 2 * kEmojiPadPx);
    }
  }
  // Glyphs the font can't render (emoji, other non-BMP/out-of-range codepoints)
  // would otherwise be drawn as mcufont's '?' fallback. Show a solid block
  // instead, advancing by the fallback width so layout/wrapping is unchanged.
  if (s->font->character_width(s->font, ch) == 0) {
    uint8_t adv = s->font->character_width(s->font, s->font->fallback_character);
    if (adv == 0) return 0;
    int16_t top = 1, h = (int16_t)s->font->height - 2;   // 1px inset top/bottom
    int16_t w = adv > 1 ? adv - 1 : adv;                  // 1px gap to next glyph
    if (h > 0 && w > 0) emit(s, x, y + top, w, h);
    return adv;
  }
  return mf_render_character(s->font, x, y, ch, mm_pixel, state);
}

bool mm_line(mf_str line, uint16_t count, void* state) {
  TextState* s = (TextState*)state;
  mf_render_aligned(s->font, s->x, s->y, MF_ALIGN_LEFT, line, count, mm_char, state);
  s->y += s->font->line_height;
  return true;
}

// Measure-only line callback: advances y past each wrapped line, draws nothing.
bool mm_measure_line(mf_str line, uint16_t count, void* state) {
  (void)line; (void)count;
  TextState* s = (TextState*)state;
  s->y += s->font->line_height;
  return true;
}

}  // namespace

static int glyphAdvance(const mf_font_s* font, mf_char ch) {
  if (s_emojiZeroWidth && s_emojiZeroWidth((uint16_t)ch)) return 0;
  uint16_t glyph;
  if (s_emojiFont && s_emojiLookup && s_emojiLookup((uint16_t)ch, glyph))
    return s_emojiFont->character_width(s_emojiFont, glyph) + 2 * kEmojiPadPx;
  return mf_character_width(font, ch);
}

int Canvas::textWidth(const mf_font_s* font, const char* str) const {
  if (!font || !str) return 0;
  // With an overlay registered, mirror mm_char so measure == render:
  // mapped codepoints measure at the overlay glyph's advance, zero-width modifiers
  // measure 0, everything else keeps stock width (incl. the fallback width for
  // unknown glyphs). No overlay -> exactly mf_get_string_width(..., false).
  if (s_emojiFont) {
    mf_str p = str; int w = 0; mf_char ch;
    while ((ch = mf_getchar(&p)) != 0) w += glyphAdvance(font, ch);
    return w;
  }
  return mf_get_string_width(font, str, 0, false);
}

int Canvas::lineHeight(const mf_font_s* font) const {
  return font ? font->line_height : 0;
}

int Canvas::fontHeight(const mf_font_s* font) const {
  return font ? font->height : 0;
}

void Canvas::drawText(const mf_font_s* font, int x, int y, const char* str,
                      DisplayDriver::Color c, TextAlign align) {
  if (!font || !str) return;
  TextState st = { this, font, c, (int16_t)x, (int16_t)y, 1, (int16_t)x, (int16_t)y };
  enum mf_align_t a = align == TextAlign::Center ? MF_ALIGN_CENTER
                    : align == TextAlign::Right  ? MF_ALIGN_RIGHT
                                                 : MF_ALIGN_LEFT;
  mf_render_aligned(font, x, y, a, str, 0, mm_char, &st);
}

int Canvas::textWidthScaled(const mf_font_s* font, const char* str, int scale) const {
  if (scale < 1) scale = 1;
  return textWidth(font, str) * scale;
}

int Canvas::fontHeightScaled(const mf_font_s* font, int scale) const {
  if (scale < 1) scale = 1;
  return fontHeight(font) * scale;
}

int Canvas::fitScale(const mf_font_s* font, const char* str, int maxW, int maxScale) const {
  if (maxScale < 1) maxScale = 1;
  if (!font || !str || maxW <= 0) return 1;
  int w1 = textWidth(font, str);
  if (w1 <= 0) return maxScale;
  int fits = maxW / w1;
  if (fits < 1) return 1;
  return fits < maxScale ? fits : maxScale;
}

void Canvas::drawTextScaled(const mf_font_s* font, int x, int y, const char* str,
                            DisplayDriver::Color c, int scale, TextAlign align) {
  if (!font || !str) return;
  if (scale < 1) scale = 1;
  if (scale == 1) { drawText(font, x, y, str, c, align); return; }
  // Alignment has to be resolved against the scaled width and then handed to
  // mcufont as a left-aligned run: it would otherwise centre the unscaled line.
  int w = textWidthScaled(font, str, scale);
  int ox = align == TextAlign::Center ? x - w / 2
         : align == TextAlign::Right  ? x - w
                                      : x;
  TextState st = { this, font, c, (int16_t)ox, (int16_t)y, scale, (int16_t)ox, (int16_t)y };
  mf_render_aligned(font, ox, y, MF_ALIGN_LEFT, str, 0, mm_char, &st);
}

int Canvas::drawTextWrapped(const mf_font_s* font, int x, int y, int w,
                            const char* str, DisplayDriver::Color c) {
  if (!font || !str) return y;
  TextState st = { this, font, c, (int16_t)x, (int16_t)y, 1, (int16_t)x, (int16_t)y };
  mf_wordwrap(font, w, str, mm_line, &st);
  return st.y;
}

int Canvas::measureTextWrapped(const mf_font_s* font, int w, const char* str) const {
  if (!font || !str) return 0;
  TextState st = { const_cast<Canvas*>(this), font, DisplayDriver::LIGHT, 0, 0, 1, 0, 0 };
  mf_wordwrap(font, w, str, mm_measure_line, &st);
  return st.y;
}

void Canvas::drawGlyph(const mf_font_s* font, int x, int y, uint16_t codepoint,
                       DisplayDriver::Color c) {
  if (!font) return;
  TextState st = { this, font, c, (int16_t)x, (int16_t)y, 1, (int16_t)x, (int16_t)y };
  mf_render_character(font, x, y, (mf_char)codepoint, mm_pixel, &st);
}

void Canvas::drawTextEllipsized(const mf_font_s* font, int x, int y, int maxWidth,
                                const char* str, DisplayDriver::Color c, TextAlign align) {
  if (!font || !str) return;
  if (textWidth(font, str) <= maxWidth) {
    drawText(font, x, y, str, c, align);
    return;
  }
  char buf[64];
  int ellw = 3 * glyphAdvance(font, '.');
  mf_str p = str;
  int len = 0, tw = 0;
  for (;;) {
    mf_str at = p;
    mf_char ch = mf_getchar(&p);
    if (ch == 0) break;
    int cw = glyphAdvance(font, ch);
    int nbytes = (int)(p - at);
    if (len + nbytes > 60 || tw + cw + ellw > maxWidth) break;
    memcpy(buf + len, at, nbytes);
    len += nbytes;
    tw += cw;
  }
  buf[len] = '.'; buf[len + 1] = '.'; buf[len + 2] = '.'; buf[len + 3] = 0;
  drawText(font, x, y, buf, c, align);
}

void Canvas::drawTextCentered(const mf_font_s* font, int x, int y, int w, int h,
                              const char* str, DisplayDriver::Color c) {
  if (!font || !str) return;
  int cy = y + (h - fontHeight(font)) / 2;
  drawText(font, x + w / 2, cy, str, c, TextAlign::Center);
}

void Canvas::drawScrollbarThumb(int x, int viewH, int contentH, int scrollPx) {
  if (contentH <= 0) return;
  int thumbH = viewH * viewH / contentH; if (thumbH < 3) thumbH = 3;
  int maxScroll = contentH - viewH;
  int thumbY = maxScroll > 0 ? (viewH - thumbH) * scrollPx / maxScroll : 0;
  fillRect(x, thumbY, 2, thumbH, DisplayDriver::LIGHT);
}

void Canvas::fillStipple(int x, int y, int w, int h, DisplayDriver::Color c) {
  // Checkerboard fill (every other pixel). Hot path: a full-screen modal scrim is
  // thousands of pixels, so clip and set the colour ONCE up front and step the inner
  // loop by 2 - instead of routing every pixel through fillRect() (which re-clips and
  // re-sets the colour each call). Same pixels, a fraction of the per-pixel overhead.
  int px = x, py = y;                                 // pre-clip origin: parity reference
  if (!clipLocal(x, y, w, h, _cl, _ct, _cr, _cb)) return;
  if (_d == nullptr) return;
  _d->setColor(themedColor(c));
  int phase = (x - px) + (y - py);                    // keep the dither stable if clipped
  for (int j = 0; j < h; j++)
    for (int i = ((phase + j) & 1); i < w; i += 2)
      _d->fillRect(_ox + x + i, _oy + y + j, 1, 1);
}

void Canvas::drawArc(int cx, int cy, int r, int thickness, int startDeg,
                     int endDeg, DisplayDriver::Color color) {
  if (r <= 0 || thickness <= 0 || endDeg <= startDeg) return;
  if (thickness > r) thickness = r;   // rr = r - t must not cross the center
  // Angular step fine enough that the outer edge has no gaps (arc length per
  // step <= ~1px): d(theta) ~ 1/r radians.
  const double stepRad = 1.0 / (double)r;
  const double a0 = startDeg * M_PI / 180.0;
  const double a1 = endDeg   * M_PI / 180.0;
  for (double a = a0; a <= a1; a += stepRad) {
    double s = sin(a), co = cos(a);
    for (int t = 0; t < thickness; t++) {
      int rr = r - t;
      int x = cx + (int)lround(rr * s);   // 0deg = top, clockwise
      int y = cy - (int)lround(rr * co);
      fillRect(x, y, 1, 1, color);        // 1x1 => FakeDisplayDriver logs a litPixel
    }
  }
}

void Canvas::blit1bpp(const uint8_t* buf, int w, int h) {
  if (_d != nullptr) _d->blitColumnMajor1bpp(buf, w, h);
}

void Canvas::drawXbm(int x, int y, const uint8_t* bits, int w, int h, DisplayDriver::Color c) {
  if (_d == nullptr) return;
  // Drivers paint an XBM in whatever setColor last left behind, so an XBM drawn
  // straight after a background fill comes out invisible. Colour it here.
  _d->setColor(themedColor(c));
  _d->drawXbm(_ox + x, _oy + y, bits, w, h);
}

}  // namespace mishmesh
