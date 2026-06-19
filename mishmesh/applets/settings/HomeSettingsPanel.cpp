#include <mishmesh/applets/settings/HomeSettingsPanel.h>
#include <mishmesh/applets/SettingsDetailApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <string.h>

namespace mishmesh {

// --- QuickActionPickerPanel ---

void QuickActionPickerPanel::begin(AppletContext&) {
  _count = 0;   // same order-sorted collection as the app menu
  for (const AppletRegistration* r = registeredApplets(); r; r = r->next) {
    if (r->placement != Placement::AppMenu || _count >= MAX_ENTRIES) continue;
    int i = _count++;
    while (i > 0 && _entries[i - 1]->order > r->order) {
      _entries[i] = _entries[i - 1];
      i--;
    }
    _entries[i] = r;
  }
  _list.setRowHeight(14);
  _list.setModel(this);
  _list.resetSelection();
}

bool QuickActionPickerPanel::radioOn(int i) const {
  return strcmp(_entries[i]->label, uiPrefs().quickActionLabel(_slot)) == 0;
}

int QuickActionPickerPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool QuickActionPickerPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select && _count > 0) {
    uiPrefs().setQuickAction(_slot, _entries[_list.selected()]->label);
    return true;
  }
  return false;   // Back bubbles -> host pops to the Home panel
}

QuickActionPickerPanel& quickActionPicker() {
  static QuickActionPickerPanel s;
  return s;
}

// --- HomeSettingsPanel ---

const char* HomeSettingsPanel::Model::label(int i) const {
  static const char* const LABELS[ROW_COUNT] = {
    "Battery percent", "Left shortcut", "Right shortcut" };
  return (i >= 0 && i < ROW_COUNT) ? LABELS[i] : "";
}

bool HomeSettingsPanel::Model::toggleState(int i) const {
  return i == BattPercent && uiPrefs().battShowPercent();
}

const char* HomeSettingsPanel::Model::value(int i) const {
  if (i == LeftAction)  return uiPrefs().quickActionLabel(UiPrefs::SLOT_LEFT);
  if (i == RightAction) return uiPrefs().quickActionLabel(UiPrefs::SLOT_RIGHT);
  return nullptr;
}

void HomeSettingsPanel::begin(AppletContext& ctx) {
  _host = ctx.host;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();   // singleton reuse: setModel skips reset on same-ptr rebind
}

int HomeSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool HomeSettingsPanel::onInput(InputEvent ev) {
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int i = _list.selected();
    if (i == Model::BattPercent) {
      uiPrefs().setBattShowPercent(!uiPrefs().battShowPercent());
    } else if (_host) {
      static SettingsDetailApplet detail;   // one level below the shared detail
      quickActionPicker().setSlot(i == Model::LeftAction ? UiPrefs::SLOT_LEFT
                                                         : UiPrefs::SLOT_RIGHT);
      detail.setPanel(&quickActionPicker());
      _host->push(&detail);
    }
    return true;
  }
  return false;   // Back bubbles
}

HomeSettingsPanel& homeSettings() {
  static HomeSettingsPanel s;
  return s;
}

}  // namespace mishmesh
