#pragma once

#include <stdint.h>

namespace mishmesh {

class Canvas;
struct AppletContext;

// A face left on the panel once the screen sleeps. Only offered on e-ink: a
// bistable panel holds whatever was last flushed, so there is something to look
// at, while turning an OLED off really does power it down.
//
// draw() returns the ms until the face wants painting again, or SLEEP_NEVER for
// a static one. That is what keeps a sleeping panel from refreshing itself - the
// face decides, so a logo costs exactly one refresh per sleep and a clock costs
// one a minute, with no host-side policy to tune.
static const int SLEEP_NEVER = -1;

// Which way round a face reads. A face that only works one way gets that way
// applied for it (the panel is turned for the sleep paint and turned back on
// wake) and offers no choice; only Either is worth asking the user about.
enum class SleepOrient : uint8_t {
  None,        // nothing to orient - the blank face
  Either,      // lays itself out to whatever it is given
  Landscape,   // needs the long edge, e.g. a 128px-wide wordmark
  Portrait,
};

struct SleepScreen {
  const char* label;
  SleepOrient orient;
  int (*draw)(Canvas& c, const AppletContext& ctx);
};

static const int SLEEP_SCREEN_COUNT = 3;

// Index 0 is the blank face, which is also the default - so unlike the other
// display prefs this one needs no index+1 sentinel: a zeroed/legacy prefs byte
// already means "screen off".
const SleepScreen* sleepScreenAt(int idx);
const char* sleepScreenLabel(int idx);
// Label table for the settings picker, in table order.
const char* const* sleepScreenLabels();

// User choice for an Either face, stored as-is in NodePrefs. 0 = Auto (follow
// the screen); 1..4 are the quarter turns of the Orientation setting, offset by
// one, so a sleep face can sit the other way up from the in-hand UI.
static const int SLEEP_ORIENT_COUNT = 5;
static const int SLEEP_ORIENT_AUTO = 0;
const char* const* sleepOrientLabels();
// Quarter turns a sleeping panel should be set to for this face and choice, or
// -1 to leave it as the user set it. uiRotation is what the UI itself uses.
int sleepOrientRotation(int faceIdx, int choice, int uiRotation);

}  // namespace mishmesh
