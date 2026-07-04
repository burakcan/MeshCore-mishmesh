#pragma once

#include <stdint.h>
#include <stddef.h>

namespace mishmesh {

class Canvas;
class StatusBar;
struct AppServices;

// Shared plumbing for the server-management applets (repeater/room panels), which
// all target a contact and all paint the same status-bar header. Extracted so the
// identical bodies aren't emitted once per panel.

// setTarget(pubKey, name): copy the 6-byte pubkey and truncate name into a fixed
// buffer. Pass sizeof(nameBuf) as nameCap.
void setTargetFields(uint8_t* pub, char* name, size_t nameCap,
                     const uint8_t* pubKey, const char* src);

// Configure + draw the top status bar (title + battery) at the top of the canvas,
// and return its height so the caller can compute its content band (top = bh + 1).
int drawTopBar(Canvas& c, StatusBar& bar, const char* title, AppServices* app, int w);

}  // namespace mishmesh
