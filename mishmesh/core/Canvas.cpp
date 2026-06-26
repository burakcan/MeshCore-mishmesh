#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/UiPrefs.h>
#include <mcufont.h>

namespace mishmesh {

// [mishmesh] glyph-overlay hook state (set via Canvas::setEmojiRenderer). Null
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
// [/mishmesh]

DisplayDriver::Color themedColor(DisplayDriver::Color c) {
  if (uiPrefs().darkMode()) return c;
  return c == DisplayDriver::LIGHT ? DisplayDriver::DARK : DisplayDriver::LIGHT;
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
};

void mm_pixel(int16_t x, int16_t y, uint8_t count, uint8_t alpha, void* state) {
  if (alpha < 128) return;
  TextState* s = (TextState*)state;
  s->c->fillRect(x, y, count, 1, s->col);
}

uint8_t mm_char(int16_t x, int16_t y, mf_char ch, void* state) {
  TextState* s = (TextState*)state;
  // [mishmesh] Glyph overlay: zero-width modifiers (VS16/ZWJ/skin tones) draw
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
  // [/mishmesh]
  // Glyphs the font can't render (emoji, other non-BMP/out-of-range codepoints)
  // would otherwise be drawn as mcufont's '?' fallback. Show a solid block
  // instead, advancing by the fallback width so layout/wrapping is unchanged.
  if (s->font->character_width(s->font, ch) == 0) {
    uint8_t adv = s->font->character_width(s->font, s->font->fallback_character);
    if (adv == 0) return 0;
    int16_t top = 1, h = (int16_t)s->font->height - 2;   // 1px inset top/bottom
    int16_t w = adv > 1 ? adv - 1 : adv;                  // 1px gap to next glyph
    if (h > 0 && w > 0) s->c->fillRect(x, y + top, w, h, s->col);
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

int Canvas::textWidth(const mf_font_s* font, const char* str) const {
  if (!font || !str) return 0;
  // [mishmesh] With an overlay registered, mirror mm_char so measure == render:
  // mapped codepoints measure at the overlay glyph's advance, zero-width modifiers
  // measure 0, everything else keeps stock width (incl. the fallback width for
  // unknown glyphs). No overlay -> exactly mf_get_string_width(..., false).
  if (s_emojiFont) {
    mf_str p = str; int w = 0; mf_char ch; uint16_t glyph;
    while ((ch = mf_getchar(&p)) != 0) {
      if (s_emojiZeroWidth && s_emojiZeroWidth((uint16_t)ch)) continue;
      if (s_emojiLookup && s_emojiLookup((uint16_t)ch, glyph))
        w += s_emojiFont->character_width(s_emojiFont, glyph) + 2 * kEmojiPadPx;
      else
        w += mf_character_width(font, ch);
    }
    return w;
  }
  // [/mishmesh]
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
  TextState st = { this, font, c, (int16_t)x, (int16_t)y };
  enum mf_align_t a = align == TextAlign::Center ? MF_ALIGN_CENTER
                    : align == TextAlign::Right  ? MF_ALIGN_RIGHT
                                                 : MF_ALIGN_LEFT;
  mf_render_aligned(font, x, y, a, str, 0, mm_char, &st);
}

int Canvas::drawTextWrapped(const mf_font_s* font, int x, int y, int w,
                            const char* str, DisplayDriver::Color c) {
  if (!font || !str) return y;
  TextState st = { this, font, c, (int16_t)x, (int16_t)y };
  mf_wordwrap(font, w, str, mm_line, &st);
  return st.y;
}

int Canvas::measureTextWrapped(const mf_font_s* font, int w, const char* str) const {
  if (!font || !str) return 0;
  TextState st = { const_cast<Canvas*>(this), font, DisplayDriver::LIGHT, 0, 0 };
  mf_wordwrap(font, w, str, mm_measure_line, &st);
  return st.y;
}

void Canvas::drawGlyph(const mf_font_s* font, int x, int y, uint16_t codepoint,
                       DisplayDriver::Color c) {
  if (!font) return;
  TextState st = { this, font, c, (int16_t)x, (int16_t)y };
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
  int ellw = textWidth(font, "...");
  int len = 0;
  for (const char* p = str; *p && len < 60; ++p) {
    buf[len] = *p;
    buf[len + 1] = 0;
    if (textWidth(font, buf) + ellw > maxWidth) { buf[len] = 0; break; }
    len++;
  }
  buf[len] = '.'; buf[len + 1] = '.'; buf[len + 2] = '.'; buf[len + 3] = 0;
  drawText(font, x, y, buf, c, align);
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

void Canvas::blit1bpp(const uint8_t* buf, int w, int h) {
  if (_d != nullptr) _d->blitColumnMajor1bpp(buf, w, h);
}

}  // namespace mishmesh
