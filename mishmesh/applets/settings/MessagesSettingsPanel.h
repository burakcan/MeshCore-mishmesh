#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

// Global Messages settings: AutoRetry + AutoResetPath toggles and the DM-acks
// value (1/2) picked via a stepper modal. Source of truth: MessagesService.
class MessagesSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Messages"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _editingAcks; }

private:
  struct Model : ListModel {
    MessagesService* svc = nullptr;
    enum Row : int { AutoRetry, AutoResetPath, DirectAcks, ROW_COUNT };
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override;
    bool isToggle(int i) const override { return i == AutoRetry || i == AutoResetPath; }
    bool toggleState(int i) const override;
    const char* value(int i) const override;   // "1"/"2" on the DirectAcks row
  } _model;

  MessagesService* _svc = nullptr;
  ListMenu         _list;
  StepperDialog    _acks;
  bool             _editingAcks = false;
};

MessagesSettingsPanel& messagesSettings();   // shared singleton

}  // namespace mishmesh
