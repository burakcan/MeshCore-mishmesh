#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/widgets/ListMenu.h>

namespace mishmesh {

class AppletHost;

// Pick-one list of AppMenu applets for a Home shortcut slot. Hosted one level
// below HomeSettingsPanel in a dedicated static SettingsDetailApplet (the
// shared settingsDetailApplet() is already on the stack hosting the parent).
class QuickActionPickerPanel : public SettingsPanel, public ListModel {
public:
  void setSlot(int slot) { _slot = slot; }   // call before begin/push

  const char* title() const override { return _slot == UiPrefs::SLOT_LEFT ? "Left shortcut" : "Right shortcut"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

  int count() const override { return _count; }
  const char* label(int i) const override { return _entries[i]->label; }
  uint16_t icon(int i) const override { return _entries[i]->icon; }
  bool isRadio(int) const override { return true; }
  bool radioOn(int i) const override;

private:
  static const int MAX_ENTRIES = 16;
  const AppletRegistration* _entries[MAX_ENTRIES];
  int _count = 0;
  int _slot = UiPrefs::SLOT_LEFT;
  ListMenu _list;
};

QuickActionPickerPanel& quickActionPicker();

// Home-face settings: battery display style and the two shortcut slots.
class HomeSettingsPanel : public SettingsPanel {
public:
  const char* title() const override { return "Home"; }
  void begin(AppletContext& ctx) override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;

private:
  struct Model : ListModel {
    enum Row : int { BattPercent, LeftAction, RightAction, ROW_COUNT };
    int count() const override { return ROW_COUNT; }
    const char* label(int i) const override;
    bool isToggle(int i) const override { return i == BattPercent; }
    bool toggleState(int i) const override;
    const char* value(int i) const override;   // current shortcut labels
  } _model;

  AppletHost* _host = nullptr;
  ListMenu _list;
};

HomeSettingsPanel& homeSettings();

}  // namespace mishmesh
