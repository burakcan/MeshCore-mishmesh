#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

class AppletHost;

// The "Experimental" settings group: low-level mesh knobs with caveats. One row
// today - "Path hash size" - which drills into PathHashApplet. Shared singleton.
class ExperimentalSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Experimental"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

  const char* rowValueForTest(int i) const { return _model.value(i); }

private:
  struct Model : ListModel {
    AppServices* app = nullptr;
    enum Row : int { PathHash, ROW_COUNT };
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override;
    const char* value(int i) const override;   // "1 byte" / "2 byte" / "3 byte"
  } _model;

  AppServices* _app  = nullptr;
  AppletHost*  _host = nullptr;
  ListMenu     _list;
};

ExperimentalSettingsPanel& experimentalSettings();   // shared singleton

}  // namespace mishmesh
