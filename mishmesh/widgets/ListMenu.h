#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>
#include <mishmesh/widgets/Marquee.h>

namespace mishmesh {

class Canvas;

// Read-only view of a list's contents, so the menu owns no storage.
struct ListModel {
  virtual ~ListModel() {}
  virtual int count() const = 0;
  virtual const char* label(int index) const = 0;
  virtual uint16_t icon(int index) const { return 0; }   // iconFont() codepoint
  virtual const char* value(int index) const { return nullptr; }  // right-aligned secondary text
  // A row may instead show an ON/OFF toggle pill in the value column.
  virtual bool isToggle(int index) const { return false; }
  virtual bool toggleState(int index) const { return false; }
  // ...or a single-select radio mark: a check at the right edge of the chosen row
  // only (no per-row pill). For "pick one" lists; isToggle/isRadio are exclusive.
  virtual bool isRadio(int index) const { return false; }
  virtual bool radioOn(int index) const { return false; }
  // A button row renders centered with a box outline (idle) or filled+inverted (selected),
  // instead of the normal left-aligned label+value layout.
  virtual bool isButton(int index) const { return false; }
};

// A ListModel backed by a fixed array of string literals - for menus whose rows
// are static labels (no icons/values/toggles). Point it at a `static const char*
// const[]` via set() instead of writing a bespoke count()/label() subclass per menu.
struct StaticListModel : ListModel {
  const char* const* _labels = nullptr;
  int _n = 0;
  StaticListModel(const char* const* labels = nullptr, int n = 0) : _labels(labels), _n(n) {}
  void set(const char* const* labels, int n) { _labels = labels; _n = n; }
  int count() const override { return _n; }
  const char* label(int i) const override {
    return (_labels && i >= 0 && i < _n) ? _labels[i] : "";
  }
};

// NavUp/NavDown move the selection (wrapping); Select is left for the owning
// applet. An optional header widget scrolls together with the rows. The selected
// row's overflowing label marquees, so re-render quickly while needsAnimation().
class ListMenu : public Widget {
  const ListModel* _model;
  int _selected;
  int _rowH;
  int _lastSel;
  Marquee _marquee;
  const char* _emptyText;   // shown centered when the model has no rows
  Widget* _header;
  int _headerH;
  // The selection highlight and the scroll offset ease toward their targets so
  // movement glides instead of snapping. _scrollPx/_barY are the animated values;
  // _scrollTarget/(_selected*_rowH) are the destinations.
  int _scrollPx, _scrollTarget;
  int _barY;            // animated highlight top, in content coords (rel. bodyTop)
  bool _animReady;      // false => snap to target on the next draw (open / wrap)
  bool _animating;      // true while the highlight or scroll is still settling
  bool _drawSelection;  // false suppresses the highlight bar (e.g. when focus is on an external widget)

  void drawRowContent(Canvas& view, int i, int ry, int cw, DisplayDriver::Color col, uint32_t now);
  void snapToTarget() { _animReady = false; }
public:
  static const int TICK_MS = 33;   // ~30 fps while a list is animating

  ListMenu()
      : _model(nullptr), _selected(0), _rowH(12), _lastSel(-1), _emptyText(nullptr),
        _header(nullptr), _headerH(0), _scrollPx(0), _scrollTarget(0),
        _barY(0), _animReady(false), _animating(false), _drawSelection(true) {}

  void setEmptyText(const char* s) { _emptyText = s; }   // caller-owned (static) string

  void setModel(const ListModel* m);     // resets selection only when the model changes
  void resetSelection() { _selected = 0; _scrollPx = _scrollTarget = _barY = 0; _lastSel = -1; _animReady = false; }
  int selected() const { return _selected; }
  void setSelected(int i) {
    if (_model && i >= 0 && i < _model->count()) { _selected = i; _lastSel = -1; _animReady = false; }
  }
  void setDrawSelection(bool v) { _drawSelection = v; }
  void setRowHeight(int h) { _rowH = h; }
  int rowHeight() const { return _rowH; }
  int firstVisibleRow(int box_height) const;   // scroll offset for the selection
  void setHeader(Widget* hdr, int height) { _header = hdr; _headerH = height; }
  bool needsAnimation() const { return _animating || _marquee.active(); }

  bool onInput(InputEvent ev) override;
  void measure(int& w, int& h) const override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
