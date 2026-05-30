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
  Widget* _header;
  int _headerH;
  int _scrollPx;

  void drawRow(Canvas& view, int i, int ry, int cw, uint32_t now);
public:
  ListMenu()
      : _model(nullptr), _selected(0), _rowH(12), _lastSel(-1),
        _header(nullptr), _headerH(0), _scrollPx(0) {}

  void setModel(const ListModel* m);     // resets selection only when the model changes
  void resetSelection() { _selected = 0; _scrollPx = 0; _lastSel = -1; }  // force back to the top
  int selected() const { return _selected; }
  void setRowHeight(int h) { _rowH = h; }
  int rowHeight() const { return _rowH; }
  int firstVisibleRow(int box_height) const;   // scroll offset for the selection
  void setHeader(Widget* hdr, int height) { _header = hdr; _headerH = height; }
  bool needsAnimation() const { return _marquee.active(); }

  bool onInput(InputEvent ev) override;
  void measure(int& w, int& h) const override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
