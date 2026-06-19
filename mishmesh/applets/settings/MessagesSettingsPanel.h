#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

class AppletHost;

// Global Messages settings: AutoRetry + AutoResetPath toggles, the DM-acks value
// (1/2) picked via a stepper modal, per-type notification sounds (push the
// shared sound picker), and a "Quick replies" row that drills into the
// canned-message manager. Source of truth: MessagesService + AppServices +
// quickReplyStore().
class MessagesSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Messages"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _editingAcks; }

  const char* rowValueForTest(int i) const { return _model.value(i); }

private:
  struct Model : ListModel {
    MessagesService* svc = nullptr;
    AppServices*     app = nullptr;
    enum Row : int { AutoRetry, AutoResetPath, DirectAcks, ChannelSound, DirectSound,
                     QuickReplies, ROW_COUNT };
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override;
    bool isToggle(int i) const override { return i == AutoRetry || i == AutoResetPath; }
    bool toggleState(int i) const override;
    const char* value(int i) const override;   // acks "1"/"2", tone names, reply count
  } _model;

  MessagesService* _svc  = nullptr;
  AppServices*     _app  = nullptr;
  AppletHost*      _host = nullptr;
  ListMenu         _list;
  StepperDialog    _acks;
  bool             _editingAcks = false;
};

MessagesSettingsPanel& messagesSettings();   // shared singleton

}  // namespace mishmesh
