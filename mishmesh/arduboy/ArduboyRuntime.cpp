#include <Arduino.h>
#include <mishmesh/arduboy/ArduboyRuntime.h>
#include <mishmesh/arduboy/ArduboyEeprom.h>
#include <mishmesh/core/Applet.h>
#include <Arduboy2Core.h>

namespace mishmesh { namespace arduboy {

void ArduboyRuntime::begin(mishmesh::AppletContext& ctx, const char* storageKey) {
  _ctx = &ctx;
  _storageKey = storageKey;
  eeprom().load(ctx.storage, storageKey);
  randomSeed(millis());
}

void ArduboyRuntime::setCanvas(mishmesh::Canvas& c) {
  coreSetCanvas(&c);
}

void ArduboyRuntime::pumpButtons() {
  if (!_ctx) return;
  coreSetButtons(_gate.buttonMask(_ctx->input()));
}

bool ArduboyRuntime::stepDue(uint32_t now) {
  return _gate.stepDue(now);
}

void ArduboyRuntime::setFrameInterval(uint32_t ms) {   // [mishmesh]
  _gate.setFrameMs(ms);
}

void ArduboyRuntime::present() {
  coreBlitCurrent();
}

void ArduboyRuntime::onButtonEvent(mishmesh::InputEvent ev) {
  if (ev == mishmesh::InputEvent::Select || ev == mishmesh::InputEvent::SelectLong) {
    _gate.pressAB();
  }
}

void ArduboyRuntime::saveIfDirty() {
  if (!_ctx) return;
  eeprom().saveIfDirty(_ctx->storage, _storageKey);
}

}}  // namespace mishmesh::arduboy
