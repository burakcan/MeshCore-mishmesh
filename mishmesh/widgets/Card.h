#pragma once

#include <mishmesh/widgets/Widget.h>
#include <mishmesh/widgets/Marquee.h>

namespace mishmesh {

// A titled card: the title sits on a filled (inverted) bar and marquees if it
// overflows; an info line below it, inside a border. Reusable detail header.
// Stores pointers to caller-owned strings.
class Card : public Widget {
  const char* _title;
  const char* _info;
  bool        _favourite;
  Marquee     _titleMarquee;
public:
  Card() : _title(""), _info(""), _favourite(false) {}
  void set(const char* title, const char* info) {
    _title = title ? title : ""; _info = info ? info : ""; _titleMarquee.reset();
  }
  void setFavourite(bool f) { _favourite = f; }
  bool needsAnimation() const { return _titleMarquee.active(); }
  int height(Canvas& c) const;
  void draw(Canvas& c, int x, int y, int w, int h) override;
};

}  // namespace mishmesh
