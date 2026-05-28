#pragma once

namespace mishmesh {

class Canvas;

// Draws the standard modal chrome over the whole canvas — a dithered scrim, a
// dithered drop shadow, and a solid bordered box — and returns the box as a
// sub-canvas (local origin at its top-left) for the caller to fill with content.
// Shared by ConfirmDialog and PingDialog.
Canvas drawModalChrome(Canvas& c);

}  // namespace mishmesh
