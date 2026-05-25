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
};

// NavUp/NavDown move the selection; Select is left for the owning applet.
class ListMenu : public Widget {
  const ListModel* _model;
  int _selected;
  int _rowH;
public:
  ListMenu() : _model(nullptr), _selected(0), _rowH(12) {}

  void setModel(const ListModel* m);     // resets selection to the top
  int selected() const { return _selected; }
  void setRowHeight(int h) { _rowH = h; }
  int rowHeight() const { return _rowH; }
  int firstVisibleRow(int box_height) const;   // scroll offset for the selection

  bool onInput(InputEvent ev) override;
  void measure(int& w, int& h) const override;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
