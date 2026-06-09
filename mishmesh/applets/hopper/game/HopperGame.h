// Converted from hopper.ino (obono/ArduboyWorks) for mishmesh.
// MIT License — see hopper/game source headers.
//
// hopperSetup()    — call once when the HopperApplet starts (replaces Arduino setup()).
// hopperLoopStep() — call once per gated frame from the applet's onRender()
//                    (replaces the body of Arduino loop(), minus the nextFrame() gate
//                    and arduboy.display() — those are handled by ArduboyRuntime).
#pragma once

void hopperSetup();
void hopperLoopStep();

// [mishmesh] Device Back button. Unwinds one level and returns true if Hopper consumed it
// (in-game -> title screen; title record/credit sub-screen -> title menu); returns false
// when there is nowhere left to go (already at the title menu, or on the logo), so the
// applet then exits to the mishmesh app menu.
bool hopperHandleBack();
