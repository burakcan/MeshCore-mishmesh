#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/Toggle.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/text/Fonts.h>

namespace mishmesh {

void ListMenu::setModel(const ListModel* m) {
  if (m == _model) return;   // re-binding the same list (e.g. returning from a drill-in) keeps position
  _model = m;
  _selected = 0;
  _scrollPx = _scrollTarget = _barY = 0;
  _animReady = false;        // snap to the top on the first draw, don't slide in
}

bool ListMenu::onInput(InputEvent ev) {
  int n = _model ? _model->count() : 0;
  if (n <= 0) return false;
  if (_selected >= n) _selected = n - 1;        // clamp a stale selection first
  // Wrapping around the ends snaps (a full-list slide would look wrong).
  if (ev == InputEvent::NavDown) { int p = _selected; _selected = (p + 1) % n; if (_selected < p) snapToTarget(); return true; }
  if (ev == InputEvent::NavUp)   { int p = _selected; _selected = (p - 1 + n) % n; if (_selected > p) snapToTarget(); return true; }
  return false;
}

void ListMenu::measure(int& w, int& h) const {
  w = 0;
  h = _headerH + _rowH * (_model ? _model->count() : 0);
}

// Kept for callers that need the row-based scroll offset (and tests).
int ListMenu::firstVisibleRow(int box_height) const {
  int n = _model ? _model->count() : 0;
  int rows = box_height / _rowH;
  if (rows < 1) rows = 1;
  int top = _selected - rows + 1;
  if (top < 0) top = 0;
  if (top > n - rows) top = n - rows;
  if (top < 0) top = 0;
  return top;
}

// Draws a row's icon/label/value in one colour, no selection fill. Called once
// per visible row in LIGHT, then again (clipped to the highlight bar) in DARK so
// text inverts cleanly as the bar slides over it.
void ListMenu::drawRowContent(Canvas& view, int i, int ry, int cw, DisplayDriver::Color col, uint32_t now) {
  int tx = 4;
  uint16_t ic = _model->icon(i);
  if (ic) {
    int iconH = view.fontHeight(iconFont());
    int iy = ry + (_rowH - iconH) / 2; if (iy < ry) iy = ry;
    view.drawGlyph(iconFont(), 2, iy, ic, col);
    tx = 2 + iconH + 3;
  }

  int ty = (_rowH - view.fontHeight(fontBody())) / 2; if (ty < 0) ty = 0;
  int rightW = 0;
  if (_model->isRadio(i)) {
    // Single-select: reserve a square slot at the right edge; draw the check only
    // on the chosen row, so the marker stays put as the cursor moves between rows.
    int gh = view.fontHeight(iconFont());
    if (_model->radioOn(i)) {
      int gy = ry + (_rowH - gh) / 2; if (gy < ry) gy = ry;
      view.drawGlyph(iconFont(), cw - gh - 2, gy, (uint16_t)Icon::Check, col);
    }
    rightW = gh + 4;
  } else if (_model->isToggle(i)) {
    const int PILL_W = 28;
    int ph = _rowH - 2;
    Toggle pill;
    pill.setOn(_model->toggleState(i));
    pill.setColor(col);
    pill.draw(view, cw - PILL_W - 2, ry + 1, PILL_W, ph);
    rightW = PILL_W + 4;
  } else {
    const char* val = _model->value(i);
    if (val && *val) {
      // Let the value take the width it needs, but never so much that the label
      // has no room (keep ~20px for it); short values are unaffected. The fixed
      // half-width cap used to clip readable values like "Not connected".
      int vw = view.textWidth(fontBody(), val);
      int maxVw = cw - tx - 20; if (maxVw < 0) maxVw = 0;
      if (vw > maxVw) vw = maxVw;
      rightW = vw + 6;
      view.drawTextEllipsized(fontBody(), cw - 2, ry + ty, vw, val, col, TextAlign::Right);
    }
  }

  int availW = cw - tx - rightW; if (availW < 0) availW = 0;
  const char* lbl = _model->label(i);
  if (i == _selected) _marquee.draw(view, fontBody(), tx, ry, availW, _rowH, lbl, col, now);
  else                view.drawTextEllipsized(fontBody(), tx, ry + ty, availW, lbl, col);
}

void ListMenu::draw(Canvas& c, int x, int y, int w, int h) {
  if (!_model) return;
  int n = _model->count();
  if (n > 0 && _selected >= n) _selected = n - 1;
  if (_selected != _lastSel) { _marquee.reset(); _lastSel = _selected; }

  uint32_t now = c.now();
  Canvas view = c.region(x, y, w, h);

  int bodyTop = _headerH + (_header && _headerH > 0 ? 2 : 0);   // gap below the header

  if (n == 0) {   // empty state: header (if any) + a centered message, no selection bar
    _animating = false;
    if (_header && _headerH > 0) _header->draw(view, 0, 0, w, _headerH);
    const char* msg = _emptyText ? _emptyText : "Empty";
    // The message may carry '\n' to force multiple lines (the bitmap font has no
    // newline glyph, so a single-line draw would show a tofu block and clip). Draw
    // each line centered and stack them, centering the whole block vertically.
    int lh = view.lineHeight(fontBody()); if (lh <= 0) lh = view.fontHeight(fontBody());
    int lines = 1; for (const char* p = msg; *p; ++p) if (*p == '\n') lines++;
    int blockH = (lines - 1) * lh + view.fontHeight(fontBody());
    int my = bodyTop + (h - bodyTop - blockH) / 2; if (my < bodyTop) my = bodyTop;
    const char* seg = msg;
    for (int li = 0; ; li++) {
      const char* nl = seg; while (*nl && *nl != '\n') nl++;
      char line[40];
      int len = (int)(nl - seg); if (len > (int)sizeof(line) - 1) len = sizeof(line) - 1;
      for (int i = 0; i < len; i++) line[i] = seg[i];
      line[len] = 0;
      view.drawTextEllipsized(fontBody(), w / 2, my + li * lh, w, line, DisplayDriver::LIGHT, TextAlign::Center);
      if (!*nl) break;
      seg = nl + 1;
    }
    return;
  }

  int contentH = bodyTop + n * _rowH;
  int maxScroll = contentH > h ? contentH - h : 0;

  // Scroll target: keep the selected row visible (computed from the committed
  // target, not the eased value, so it doesn't chatter while settling).
  int selTop = bodyTop + _selected * _rowH;
  int selBot = selTop + _rowH;
  if (selBot > _scrollTarget + h) _scrollTarget = selBot - h;
  if (selTop < _scrollTarget) _scrollTarget = selTop;
  if (_selected == 0) _scrollTarget = 0;     // first row: reveal the header
  if (_scrollTarget > maxScroll) _scrollTarget = maxScroll;
  if (_scrollTarget < 0) _scrollTarget = 0;
  int barTarget = _selected * _rowH;         // highlight top, content coords

  if (!_animReady) {                         // open / wrap: jump straight to target
    _scrollPx = _scrollTarget; _barY = barTarget; _animReady = true;
  } else {
    int minStep = _rowH / 2; if (minStep < 3) minStep = 3;   // ~half a row per frame
    _scrollPx = approach(_scrollPx, _scrollTarget, minStep);
    _barY = approach(_barY, barTarget, minStep);
  }
  _animating = (_scrollPx != _scrollTarget) || (_barY != barTarget);

  bool scrollbar = contentH > h;
  int cw = scrollbar ? w - 4 : w;

  if (_header && _headerH > 0) {
    int hy = -_scrollPx;
    if (hy + _headerH > 0 && hy < h) _header->draw(view, 0, hy, cw, _headerH);
  }

  // Base pass: every visible row in LIGHT (un-inverted).
  for (int i = 0; i < n; i++) {
    int ry = bodyTop + i * _rowH - _scrollPx;
    if (ry + _rowH <= 0 || ry >= h) continue;
    drawRowContent(view, i, ry, cw, DisplayDriver::LIGHT, now);
  }

  // Highlight bar, then re-draw the rows it overlaps in DARK clipped to it, so
  // text inverts pixel-by-pixel as the bar glides between rows.
  int barY = bodyTop + _barY - _scrollPx;
  view.fillRect(0, barY, cw, _rowH, DisplayDriver::LIGHT);
  for (int i = 0; i < n; i++) {
    int ry = bodyTop + i * _rowH - _scrollPx;
    if (ry + _rowH <= barY || ry >= barY + _rowH) continue;   // no overlap with the bar
    Canvas bar = view.region(0, barY, cw, _rowH);
    drawRowContent(bar, i, ry - barY, cw, DisplayDriver::DARK, now);
  }

  if (scrollbar) {
    int thumbH = h * h / contentH; if (thumbH < 3) thumbH = 3;
    int thumbY = maxScroll > 0 ? (h - thumbH) * _scrollPx / maxScroll : 0;
    view.fillRect(w - 2, thumbY, 2, thumbH, DisplayDriver::LIGHT);
  }
}

}  // namespace mishmesh
