// mishmesh/applets/RepeaterSettingsPanel.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/RemoteSettings.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>
#include <mishmesh/widgets/StatusBar.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

// Generic remote-settings panel: renders any SettingFieldDef[] (label + fetched value)
// as a ListMenu (label+value rows + a "Save" button row at index _n). Drives RemoteSettingsEngine,
// edits fields via the keypad/toggle, and Saves via the button at index _n. A read-only-only
// field set (e.g. Version) renders as plain ScrollText instead of a form. Back with
// unsaved edits raises a Discard confirm. One shared static staged buffer (only one panel
// is open at a time). Host-safe.
class RepeaterSettingsPanel : public Applet, public ListModel {
public:
  static const int MAX_FIELDS = 4;
  static const int FIELD_CAP  = 132;   // owner.info (120) / public key (64 hex) both fit

  RepeaterSettingsPanel() : Applet("Settings") {}
  void setTarget(const uint8_t* pubKey, const char* name);
  void setModel(const SettingFieldDef* defs, int n, const char* title);   // call before push
  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

  int  fieldCountForTest() const { return _n; }             // field count (_n)
  bool hasButtonForTest() const { return hasEditable(); }   // Save button present when editable
  int  focusForTest() const { return _view.selected(); }   // current list focus (Save at _n)
  const char* valueForTest(int i) const { return _engine.value(i); }
  const char* displayValueForTest(int i) const;             // display string (truncated for longValue)
  const char* textViewLineForTest(int i) const { return _textView.lineForTest(i); }
  bool anyDirtyForTest() const { return _engine.anyDirty(); }
  bool editingForTest() const { return _phase == Phase::Editing; }
  bool hasEditableForTest() const { return hasEditable(); }
  bool inModalForTest() const { return _phase == Phase::Modal; }

  // ListModel (form-mode field rows + Save button at _n)
  int count() const override { return hasEditable() ? _n + 1 : _n; }
  const char* label(int i) const override { return (i >= 0 && i < _n) ? _defs[i].label : "Save"; }
  const char* value(int i) const override { return (i >= 0 && i < _n) ? displayValueForTest(i) : nullptr; }
  bool isButton(int i) const override { return hasEditable() && i == _n; }

private:
  enum class Phase : uint8_t { List, Editing, Confirm, Modal };

  AppletHost*      _host = nullptr;
  ContactsService* _svc = nullptr;
  AppServices*     _app = nullptr;
  uint8_t  _pub[6] = {0};
  char     _name[32] = {0};
  const SettingFieldDef* _defs = nullptr;
  int      _n = 0;
  const char* _title = "Settings";
  StatusBar _bar;
  int      _editIdx = -1;
  Phase    _phase = Phase::List;
  bool     _textBuilt = false;   // true once the read-only text view has been populated
  RemoteSettingsEngine _engine;
  ListMenu _view;
  ScrollText _textView;    // used when !hasEditable() (e.g. Version panel)
  ScrollText _modalText;   // used for longValue ReadOnly field pop-up
  ConfirmDialog _confirm;
  // Single shared preview buffer; valid because a panel has at most one longValue field (a second would clobber it).
  mutable char _truncBuf[12];   // "XXXXXX..." preview
  // Keypad stores its title by pointer, so it must outlive editField()'s stack frame; hold it here.
  char _editTitle[40];

  bool hasEditable() const;
  void editField(int i);
  void openModal(int i);
  static void onEditDone(void* ctx, const char* text);
};

RepeaterSettingsPanel& repeaterSettingsPanel();

}  // namespace mishmesh
