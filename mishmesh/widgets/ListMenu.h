#pragma once

#include <mishmesh/widgets/Widget.h>

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

// NavUp/NavDown move the selection; Select is left for the owning applet.
// The selected row's overflowing label marquees, so the owner should re-render
// quickly while needsAnimation() is true.
class ListMenu : public Widget {
  const ListModel* _model;
  int _selected;
  int _rowH;
  mutable bool _animating;     // selected label is currently marqueeing
  mutable int _lastSel;        // selection at last draw, to detect a change
  mutable uint32_t _selStart;  // frame time the current row became selected (marquee phase origin)
  mutable uint32_t _lastDraw;  // frame time of the last draw, to detect re-entry
public:
  ListMenu() : _model(nullptr), _selected(0), _rowH(12), _animating(false),
               _lastSel(-1), _selStart(0), _lastDraw(0) {}

  void setModel(const ListModel* m);     // resets selection to the top
  int selected() const { return _selected; }
  void setRowHeight(int h) { _rowH = h; }
  int rowHeight() const { return _rowH; }
  int firstVisibleRow(int box_height) const;   // scroll offset for the selection
  bool needsAnimation() const { return _animating; }

  bool onInput(InputEvent ev) override;
  void measure(int& w, int& h) const override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
