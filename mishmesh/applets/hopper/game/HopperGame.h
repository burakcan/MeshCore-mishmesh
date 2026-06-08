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

// [mishmesh] State access for the applet's Back button: while a game is being played,
// Back returns to Hopper's own title screen (its menu); at the title, Back exits the applet.
bool hopperInGame();
void hopperReturnToTitle();
