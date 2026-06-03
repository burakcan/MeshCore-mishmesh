#pragma once

namespace mishmesh {

class Canvas;

// Draws a solid bordered box centred on the canvas (no scrim, no shadow - both were
// removed as too costly per frame on mono OLED) and returns it as a sub-canvas
// (local origin at its top-left) for the caller to fill with content. Shared by all
// modals and dialogs so they look consistent.
Canvas drawModalChrome(Canvas& c);

}  // namespace mishmesh
