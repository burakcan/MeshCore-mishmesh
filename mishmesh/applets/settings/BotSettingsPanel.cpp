#include <mishmesh/applets/settings/BotSettingsPanel.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletHost.h>
#include <stdio.h>

namespace mishmesh {

void BotSettingsPanel::begin(AppletContext& ctx) {
  _app = ctx.app;
  _host = ctx.host;
  _model.app = _app;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
}

int BotSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

void BotSettingsPanel::onChannelDone(void* ctx, const char* text) {
  BotSettingsPanel* self = static_cast<BotSettingsPanel*>(ctx);
  if (self && self->_app) self->_app->setBotChannel(text);
}

void BotSettingsPanel::onTriggerDone(void* ctx, const char* text) {
  BotSettingsPanel* self = static_cast<BotSettingsPanel*>(ctx);
  if (self && self->_app) self->_app->setBotTrigger(text);
}

void BotSettingsPanel::onReplyDone(void* ctx, const char* text) {
  BotSettingsPanel* self = static_cast<BotSettingsPanel*>(ctx);
  if (self && self->_app) self->_app->setBotReply(text);
}

bool BotSettingsPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev != InputEvent::Select || !_app) return false;

  int i = _list.selected();
  if (i == Model::Enabled) {
    _app->setBotEnabled(!_app->botEnabled());
  } else if (i == Model::Commands) {
    _app->setBotCommandsEnabled(!_app->botCommandsEnabled());
  } else if (i == Model::Location) {
    _app->setBotLocationEnabled(!_app->botLocationEnabled());
  } else if (i == Model::Mode) {
    _app->setBotMode((_app->botMode() + 1) % 3);
  } else if (i == Model::Channel && _host) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%s", _app->botChannel());
    keypadApplet().configure(buf, sizeof(buf) - 1, "Bot channel",
                             &BotSettingsPanel::onChannelDone, this);
    _host->push(&keypadApplet());
  } else if (i == Model::Trigger && _host) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%s", _app->botTrigger());
    keypadApplet().configure(buf, sizeof(buf) - 1, "Bot trigger",
                             &BotSettingsPanel::onTriggerDone, this);
    _host->push(&keypadApplet());
  } else if (i == Model::Text && _host) {
    static char buf[140];
    snprintf(buf, sizeof(buf), "%s", _app->botReply()[0] ? _app->botReply() : "test");
    keypadApplet().configure(buf, sizeof(buf) - 1, "Bot text",
                             &BotSettingsPanel::onReplyDone, this);
    _host->push(&keypadApplet());
  } else {
    uint8_t bit = Model::fieldBit(i);
    if (bit) _app->setBotReplyFields(_app->botReplyFields() ^ bit);
  }
  return true;
}

BotSettingsPanel& botSettings() {
  static BotSettingsPanel s;
  return s;
}

} // namespace mishmesh
