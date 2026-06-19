#include <mishmesh/applets/settings/MessagesSettingsPanel.h>
#include <mishmesh/applets/settings/QuickRepliesPanel.h>
#include <mishmesh/applets/SettingsDetailApplet.h>
#include <mishmesh/applets/SoundPickerApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/QuickReplyStore.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/sound/Sounds.h>
#include <stdio.h>

namespace mishmesh {

static void acksLabel(int v, char* out, uint16_t cap) {
  snprintf(out, cap, "%d", v);
}

const char* MessagesSettingsPanel::Model::label(int i) const {
  static const char* LABELS[ROW_COUNT] = { "Auto retry DMs", "Auto reset DM paths",
                                           "Direct msg acks", "Channel sound",
                                           "Direct sound", "Quick replies" };
  return (i >= 0 && i < ROW_COUNT) ? LABELS[i] : "";
}

bool MessagesSettingsPanel::Model::toggleState(int i) const {
  if (!svc) return false;
  MessagesConfig c = svc->getMessagesConfig();
  if (i == AutoRetry)     return c.autoRetry;
  if (i == AutoResetPath) return c.autoResetPath;
  return false;
}

const char* MessagesSettingsPanel::Model::value(int i) const {
  static char buf[8];
  if (i == DirectAcks && svc) {
    snprintf(buf, sizeof(buf), "%u", svc->getMessagesConfig().directAcks);
    return buf;
  }
  if ((i == ChannelSound || i == DirectSound) && app) {
    bool channel = i == ChannelSound;
    return sound::notifyToneEncodedName(app->notifyTone(channel), false,
                                        sound::notifyTypeDefault(channel));
  }
  if (i == QuickReplies) {   // show how many are configured
    snprintf(buf, sizeof(buf), "%d", quickReplyStore().count());
    return buf;
  }
  return nullptr;
}

void MessagesSettingsPanel::begin(AppletContext& ctx) {
  _svc = ctx.messages;
  _app = ctx.app;
  _host = ctx.host;
  _model.svc = _svc;
  _model.app = _app;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();   // singleton reuse: setModel skips reset on same-ptr rebind
  _editingAcks = false;
}

int MessagesSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  if (_editingAcks) { _acks.draw(c, 0, 0, c.width(), c.height()); return 100; }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool MessagesSettingsPanel::onInput(InputEvent ev) {
  if (_editingAcks) {
    if (_acks.onInput(ev)) {
      StepperResult r = _acks.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed && _svc) {
          MessagesConfig cfg = _svc->getMessagesConfig();
          cfg.directAcks = (uint8_t)_acks.value();
          _svc->setMessagesConfig(cfg);
        }
        _editingAcks = false;
        _acks.reset();
      }
    }
    return true;   // swallow everything while modal
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int i = _list.selected();
    MessagesConfig cfg = _svc ? _svc->getMessagesConfig() : MessagesConfig();
    if (_model.isToggle(i) && _svc) {
      if (i == Model::AutoRetry)          cfg.autoRetry = !cfg.autoRetry;
      else if (i == Model::AutoResetPath) cfg.autoResetPath = !cfg.autoResetPath;
      _svc->setMessagesConfig(cfg);
    } else if (i == Model::DirectAcks && _svc) {
      _acks.configure("DM acks", cfg.directAcks, 1, 2, acksLabel);
      _editingAcks = true;
    } else if ((i == Model::ChannelSound || i == Model::DirectSound) && _host) {
      bool channel = i == Model::ChannelSound;
      soundPickerApplet().setGlobal(channel, channel ? "Channel msgs" : "Direct msgs");
      _host->push(&soundPickerApplet());
    } else if (i == Model::QuickReplies && _host) {
      // Drill into the canned-message manager. A dedicated detail-applet instance
      // (not the shared settingsDetailApplet, which is already on the stack hosting
      // THIS panel) hosts the QuickRepliesPanel one level deeper.
      static SettingsDetailApplet detail;
      detail.setPanel(&quickRepliesSettings());
      _host->push(&detail);
    }
    return true;
  }
  return false;   // Back bubbles
}

MessagesSettingsPanel& messagesSettings() {
  static MessagesSettingsPanel s;
  return s;
}

}  // namespace mishmesh
