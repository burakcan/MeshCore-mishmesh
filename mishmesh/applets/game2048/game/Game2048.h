// Entry points for the mishmesh 2048 applet (replaces the upstream .ino setup()/loop()).
//   game2048Setup()    — call once from Game2048Applet::onStart.
//   game2048LoopStep() — call once per gated frame from onRender (runs one StepGame()).
//   game2048NewGame()  — start a fresh game (driven by the applet's restart-confirm).
//   game2048Persist()  — force-save the current board+state so it resumes from the menu.
#pragma once

void game2048Setup();
void game2048LoopStep();
void game2048NewGame();
void game2048Persist();
