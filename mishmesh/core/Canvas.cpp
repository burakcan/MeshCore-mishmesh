#include <mishmesh/core/Canvas.h>
#include <mcufont.h>

namespace mishmesh {

// Intersect a local rect with (0,0,bw,bh); false if nothing remains visible.
static bool clipLocal(int& x, int& y, int& w, int& h, int bw, int bh) {
  if (w <= 0 || h <= 0) return false;
  int x2 = x + w;
  int y2 = y + h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x2 > bw) x2 = bw;
  if (y2 > bh) y2 = bh;
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
  return Canvas(_d, _ox + x, _oy + y, cw, ch, _now);
}

void Canvas::fillRect(int x, int y, int w, int h, DisplayDriver::Color c) {
  if (!clipLocal(x, y, w, h, _w, _h)) return;
  if (_d) {
    _d->setColor(c);
    _d->fillRect(_ox + x, _oy + y, w, h);
  }
}

void Canvas::drawRect(int x, int y, int w, int h, DisplayDriver::Color c) {
  if (!clipLocal(x, y, w, h, _w, _h)) return;
  if (_d) {
    _d->setColor(c);
    _d->drawRect(_ox + x, _oy + y, w, h);
  }
}

void Canvas::text(int x, int y, const char* str, DisplayDriver::Color c) {
  if (!_d || str == nullptr) return;
  if (x < 0 || y < 0 || x >= _w || y >= _h) return;
  _d->setColor(c);
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
  return mf_render_character(s->font, x, y, ch, mm_pixel, state);
}

bool mm_line(mf_str line, uint16_t count, void* state) {
  TextState* s = (TextState*)state;
  mf_render_aligned(s->font, s->x, s->y, MF_ALIGN_LEFT, line, count, mm_char, state);
  s->y += s->font->line_height;
  return true;
}

}  // namespace

int Canvas::textWidth(const mf_font_s* font, const char* str) const {
  if (!font || !str) return 0;
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
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      if (((i + j) & 1) == 0) fillRect(x + i, y + j, 1, 1, c);
}

}  // namespace mishmesh
