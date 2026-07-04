#include <mishmesh/applets/settings/QuickRepliesPanel.h>
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/Modal.h>
#include <string.h>

namespace mishmesh {

uint16_t QuickRepliesPanel::Model::icon(int i) const {
  return isAddRow(i) ? (uint16_t)Icon::Plus : 0;
}

void QuickRepliesPanel::begin(AppletContext& ctx) {
  _host = ctx.host;
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();
  _menuOpen = _confirming = _moving = false;
  _itemIdx = _moveOrig = _editIdx = -1;
}

void QuickRepliesPanel::onShow() {
  _list.resetSelection();   // singleton: back to top each open
  _menuOpen = _confirming = _moving = false;
}

int QuickRepliesPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  // Grab-and-move: reserve a bottom strip for a "you are moving this row" hint so
  // move mode is unmistakable (the selection bar alone looks like normal browsing).
  if (_moving) {
    const int HINT_H = 11;
    _list.draw(c, x, y, w, h - HINT_H);
    c.fillRect(x, y + h - HINT_H, w, HINT_H, DisplayDriver::DARK);
    c.drawText(fontCaption(), x + w / 2, y + h - HINT_H + 2, "Moving: Up/Dn, OK drops",
               DisplayDriver::LIGHT, TextAlign::Center);
    return _list.needsAnimation() ? ListMenu::TICK_MS : 200;
  }
  _list.draw(c, x, y, w, h);
  if (_confirming) { _confirm.draw(c, 0, 0, c.width(), c.height()); return 100; }
  if (_menuOpen) {
    Canvas box = drawModalChrome(c);
    _actions.draw(box, 2, 2, box.width() - 4, box.height() - 4);
    return _actions.needsAnimation() ? ListMenu::TICK_MS : 250;
  }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

void QuickRepliesPanel::onEditDone(void* ctx, const char* text) {
  auto* self = static_cast<QuickRepliesPanel*>(ctx);
  if (!text || !text[0]) return;                  // blank -> no-op
  if (self->_editIdx < 0) quickReplyStore().add(text);
  else                    quickReplyStore().set(self->_editIdx, text);
}

bool QuickRepliesPanel::onInput(InputEvent ev) {
  // Delete confirm.
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _itemIdx >= 0) quickReplyStore().remove(_itemIdx);
        _confirming = false; _itemIdx = -1;
      }
    }
    return true;
  }

  // Grab-and-move: NavUp/Down slide the grabbed row; Select drops; Back reverts.
  if (_moving) {
    int i = _list.selected();
    if (ev == InputEvent::NavUp && i > 0) {
      quickReplyStore().move(i, i - 1); _list.setSelected(i - 1); return true;
    }
    if (ev == InputEvent::NavDown && i < quickReplyStore().count() - 1) {
      quickReplyStore().move(i, i + 1); _list.setSelected(i + 1); return true;
    }
    if (ev == InputEvent::Select) {   // drop: commit the batched reorder once
      quickReplyStore().save();
      _moving = false; _moveOrig = -1; return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) {
      // Cancel: revert in memory to the grab origin. Nothing was persisted during
      // the drag, so disk already holds the original order - no save() needed.
      if (i != _moveOrig) { quickReplyStore().move(i, _moveOrig); _list.setSelected(_moveOrig); }
      _moving = false; _moveOrig = -1; return true;
    }
    return true;   // swallow everything else while grabbed
  }

  // Action menu (Edit / Move / Delete).
  if (_menuOpen) {
    if (_actions.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      int a = _actions.selected();
      _menuOpen = false;
      if (a == 0) {                       // Edit
        _editIdx = _itemIdx;
        strncpy(_editBuf, quickReplyStore().text(_itemIdx), QuickReplyStore::MAX_LEN);
        _editBuf[QuickReplyStore::MAX_LEN] = 0;
        keypadApplet().configure(_editBuf, QuickReplyStore::MAX_LEN, "Reply", &onEditDone, this);
        if (_host) _host->push(&keypadApplet());
      } else if (a == 1) {                // Move
        _list.setSelected(_itemIdx);
        _moveOrig = _itemIdx;
        _moving = true;
      } else {                            // Delete
        _confirm.configure("Delete this reply?");
        _confirming = true;
      }
      return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _menuOpen = false; return true; }
    return true;
  }

  // Normal list.
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int i = _list.selected();
    if (_model.isAddRow(i)) {             // Add reply...
      _editIdx = -1;
      _editBuf[0] = 0;
      keypadApplet().configure(_editBuf, QuickReplyStore::MAX_LEN, "Reply", &onEditDone, this);
      if (_host) _host->push(&keypadApplet());
    } else {                              // open action menu for this reply
      _itemIdx = i;
      static const char* const ACTION_LABELS[] = {"Edit", "Move", "Delete"};
      _actionModel.set(ACTION_LABELS, 3);
      _actions.setModel(&_actionModel);
      _actions.setRowHeight(14);
      _actions.resetSelection();
      _menuOpen = true;
    }
    return true;
  }
  return false;   // Back bubbles -> host pops
}

QuickRepliesPanel& quickRepliesSettings() {
  static QuickRepliesPanel s;
  return s;
}

}  // namespace mishmesh
