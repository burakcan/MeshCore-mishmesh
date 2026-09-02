#pragma once

struct mf_font_s;

namespace mishmesh {

class Canvas;
typedef struct ::mf_font_s Font;

// Every fixed pixel constant in the widget set was picked for a 128x64 panel.
// The e-ink builds hand us 250x122 or 122x250 at Standard interface size, where
// those constants strand a list in the top third of the glass. One tier decision
// lives here instead of a `height() >= 100` test copied into each applet.
//
// The SHORT edge decides: a 61x125 portrait canvas is tall but has no width to
// spend, so it stays compact.
bool isRegularCanvas(const Canvas& c);

// Taller than wide. Screens that split their content sideways (QR beside a
// caption, session ring beside a readout) must stack instead - halving a 122px
// width leaves neither column usable.
bool isPortrait(const Canvas& c);

// Added to a widget's authored row height. Applets size their rows for the
// compact panel and this opens them up on a bigger one, so no call site changes.
int rowHeightBonus(const Canvas& c);

// Status/tab bar height, from the applet's authored compact height.
int barHeight(const Canvas& c, int base);

// Prose tier: a 64px panel only has room for the caption font; a taller one can
// afford body type, which is what the rest of the UI is set in.
const Font* tierFont(const Canvas& c);

// Ceiling for a magnified readout (clock, timer, stopwatch). This is only the
// vertical budget - pair it with Canvas::fitScale to clamp against the width,
// or the readout runs off both edges of a portrait panel.
int numScaleCap(const Canvas& c);

}  // namespace mishmesh
