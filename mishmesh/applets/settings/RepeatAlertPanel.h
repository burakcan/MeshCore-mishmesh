#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StepperDialog.h>

namespace mishmesh {

// "Repeat alert": how often an unread message re-chirps, and when it gives up.
// Drilled into from the Messages panel. Both values are minutes in
// MessagesConfig; the steppers walk a fixed option table rather than every
// integer. Shared singleton (see repeatAlertSettings()).
class RepeatAlertPanel : public SettingsPanel {
public:
  const char* title() const override { return "Repeat alert"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _editing != Row::None; }

  const char* rowValueForTest(int i) const { return _model.value(i); }

private:
  enum class Row : int { Every = 0, StopAfter = 1, None = -1 };

  struct Model : ListModel {
    MessagesService* svc = nullptr;
    int count() const override { return 2; }
    const char* label(int i) const override;
    const char* value(int i) const override;
  } _model;

  MessagesService* _svc = nullptr;
  ListMenu         _list;
  StepperDialog    _stepper;
  Row              _editing = Row::None;
};

RepeatAlertPanel& repeatAlertSettings();

// Minutes -> row label, shared with the Messages panel's summary row.
void repeatIntervalLabel(int mins, char* out, uint16_t cap);
void repeatStopLabel(int mins, char* out, uint16_t cap);

}  // namespace mishmesh
