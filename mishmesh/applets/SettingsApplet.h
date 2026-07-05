#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {

// App-menu entry. Lists the settings sections; selecting one pushes the shared
// SettingsDetailApplet bound to that section's panel. Entries are gated by an
// availability predicate so build/hardware-specific sections only appear when
// usable. Add a section by appending one row to ENTRIES.
class SettingsApplet : public Applet {
public:
  SettingsApplet() : Applet("Settings") {}
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int  entryCountForTest() const { return _visibleCount; }

private:
  struct Entry {
    const char*    label;
    SettingsPanel* (*panel)();
    uint16_t       icon;
    bool           (*available)(const AppletContext&);
  };
  static const int ENTRY_COUNT = 8;
  static const Entry ENTRIES[ENTRY_COUNT];

  struct Model : ListModel {
    const SettingsApplet* owner = nullptr;
    int count() const override;
    const char* label(int i) const override;
    uint16_t    icon(int i)  const override;
  } _model;

  AppletHost*  _host = nullptr;
  AppServices* _app  = nullptr;
  StatusBar    _bar;
  ListMenu     _list;
  int          _visible[ENTRY_COUNT];   // visible row -> ENTRIES index
  int          _visibleCount = 0;

  friend struct Model;
};

}  // namespace mishmesh
