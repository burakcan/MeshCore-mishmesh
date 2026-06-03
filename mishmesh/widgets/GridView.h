#pragma once

#include <stdint.h>
#include <mishmesh/widgets/Widget.h>

namespace mishmesh {

class Canvas;

// Read-only view of a 2D grid's contents, so the view owns no storage. The
// sibling of ListModel for two-dimensional, focus-navigated layouts.
struct GridModel {
  virtual ~GridModel() {}
  virtual int rows() const = 0;
  virtual int cols() const = 0;
  virtual const char* cellLabel(int r, int c) const = 0;   // text, or nullptr if icon-only
  virtual uint16_t cellIcon(int r, int c) const { return 0; }  // iconFont() codepoint, 0 = none
};

// A navigable grid of cells with a focus highlight. Nav* move focus (wrapping at
// edges); Select is left for the owning applet (same contract as ListMenu). The
// focused cell is drawn inverted (LIGHT fill + DARK content).
class GridView : public Widget {
  const GridModel* _model;
  int _row, _col;
public:
  GridView() : _model(nullptr), _row(0), _col(0) {}

  void setModel(const GridModel* m);
  int  focusedRow() const { return _row; }
  int  focusedCol() const { return _col; }
  void setFocus(int r, int c) { _row = r; _col = c; }

  bool onInput(InputEvent ev) override;
  void measure(int& w, int& h) const override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
