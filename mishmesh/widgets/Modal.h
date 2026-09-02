#pragma once

namespace mishmesh {

class Canvas;

// Draws a solid bordered box centred on the canvas (no scrim, no shadow - both were
// removed as too costly per frame on mono OLED) and returns it as a sub-canvas
// (local origin at its top-left) for the caller to fill with content. Shared by all
// modals and dialogs so they look consistent.
//
// wantW/wantH are the content size the caller can actually fill, in px. The box is
// the smaller of that (plus the border) and the canvas inset by 8px on each edge,
// so a dialog holding two lines stays two lines tall instead of stretching to a
// 234px-high frame with its content stranded at the extremes. Pass 0 for an axis
// with no natural size (a scrolling list) to keep the full inset on it.
Canvas drawModalChrome(Canvas& c, int wantW = 0, int wantH = 0);

}  // namespace mishmesh
