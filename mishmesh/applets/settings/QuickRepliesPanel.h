#pragma once

#include <mishmesh/core/SettingsPanel.h>
#include <mishmesh/core/QuickReplyStore.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ConfirmDialog.h>

namespace mishmesh {

class AppletHost;

// Manage up to 10 quick replies: add/edit (keypad), delete (confirm), reorder
// (grab-and-move). Shared singleton so the in-app Settings tab and the Settings
// app detail screen edit one instance. Source of truth: quickReplyStore().
class QuickRepliesPanel : public SettingsPanel {
public:
  const char* title() const override { return "Quick replies"; }
  void begin(AppletContext& ctx) override;
  void onShow() override;
  int  renderBody(Canvas& c, int x, int y, int w, int h) override;
  bool onInput(InputEvent ev) override;
  bool modalActive() const override { return _menuOpen || _confirming || _moving; }

  // Keypad completion (add when _editIdx < 0, else overwrite _editIdx).
  static void onEditDone(void* ctx, const char* text);

private:
  // Rows: one per reply, plus a trailing "Add reply..." row while under the cap.
  struct Model : ListModel {
    bool canAdd() const { return quickReplyStore().count() < QuickReplyStore::MAX_ITEMS; }
    int count() const override { return quickReplyStore().count() + (canAdd() ? 1 : 0); }
    bool isAddRow(int i) const { return i == quickReplyStore().count(); }
    const char* label(int i) const override {
      return isAddRow(i) ? "Add reply..." : quickReplyStore().text(i);
    }
    uint16_t icon(int i) const override;   // Plus on the add row only
  } _model;

  // Per-item action menu (Edit / Move / Delete).
  struct ActionModel : ListModel {
    int count() const override { return 3; }
    const char* label(int i) const override {
      return i == 0 ? "Edit" : i == 1 ? "Move" : "Delete";
    }
  } _actionModel;

  AppletHost*   _host = nullptr;
  ListMenu      _list;
  ListMenu      _actions;
  ConfirmDialog _confirm;

  bool _menuOpen   = false;  // action menu overlay open
  bool _confirming = false;  // delete confirm open
  bool _moving     = false;  // grab-and-move active
  int  _itemIdx    = -1;     // reply the action menu / confirm targets
  int  _moveOrig   = -1;     // grab start index, for Back-to-cancel revert
  int  _editIdx    = -1;     // keypad target: -1 = add, else overwrite
  char _editBuf[QuickReplyStore::MAX_LEN + 1] = {0};
};

QuickRepliesPanel& quickRepliesSettings();

}  // namespace mishmesh
