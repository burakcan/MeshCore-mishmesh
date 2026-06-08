// Converted from hopper.ino (obono/ArduboyWorks).
// MIT License. Original copyright (c) 2017 obono.
//
// Adaptation for mishmesh (Task 7):
//   - setup()/loop() renamed to hopperSetup()/hopperLoopStep().
//   - arduboy.nextFrame() gate removed (ArduboyRuntime::stepDue() gates externally).
//   - arduboy.snapshotButtons() inserted to update button state each step (normally
//     a side-effect of nextFrame(); required when the gate is external).
//   - arduboy.display() removed (ArduboyRuntime::present() handles the blit).
//   - DEBUG serial blocks omitted (no Serial on mishmesh companion radio).
// All game logic is verbatim from upstream.

#include "common.h"
#include "HopperGame.h"

#if ARDUBOY_LIB_VER < 10101
#error
#endif // It may work even if you use other version.

/*  Defines  */

enum MODE {
    LOGO_MODE = 0,
    TITLE_MODE,
    GAME_MODE
};

/*  Typedefs  */

typedef struct {
    void (*initFunc)(void);
    bool (*updateFunc)(void);
    void (*drawFunc)(void);
} MODULE_FUNCS;

/*  Global Variables  */

MyArduboy arduboy;

/*  Local Variables  */

static const MODULE_FUNCS moduleTable[] = {
    { initLogo,  updateLogo,  drawLogo },
    { initTitle, updateTitle, drawTitle },
    { initGame,  updateGame,  drawGame },
};

static MODE mode = LOGO_MODE;

/*---------------------------------------------------------------------------*/

void hopperSetup()
{
    arduboy.beginNoLogo();
    arduboy.setFrameRate(60);
    moduleTable[LOGO_MODE].initFunc();
}

// One game frame: snapshot buttons, run the active module's update+draw,
// advance mode on completion. Called by HopperApplet::onRender() only when
// ArduboyRuntime::stepDue() is true; arduboy.display() is not called here —
// ArduboyRuntime::present() blits the framebuffer to the Canvas every pass.
void hopperLoopStep()
{
    arduboy.snapshotButtons();
    bool isDone = moduleTable[mode].updateFunc();
    moduleTable[mode].drawFunc();
    if (isDone) {
        mode = (mode == TITLE_MODE) ? GAME_MODE : TITLE_MODE;
        moduleTable[mode].initFunc();
        dprint("mode=");
        dprintln(mode);
    }
}

// [mishmesh] Back-button support for the mishmesh applet (see HopperGame.h). The applet
// drives these from onInput(); they run between gated frames, which is safe under the
// cooperative single-threaded loop (onInput dispatches before onRender in the same pass).
bool hopperInGame()
{
    return mode == GAME_MODE;
}

void hopperReturnToTitle()
{
    mode = TITLE_MODE;
    moduleTable[TITLE_MODE].initFunc();
}
