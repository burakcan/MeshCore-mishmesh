#include <mishmesh/applets/settings/ContactsSettingsPanel.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <stdio.h>

namespace mishmesh {

static void maxHopsLabel(int raw, char* out, uint16_t cap) {
  if (raw <= 0)       snprintf(out, cap, "No limit");
  else if (raw == 1)  snprintf(out, cap, "Direct");
  else if (raw == 2)  snprintf(out, cap, "1 hop");
  else                snprintf(out, cap, "%d hops", raw - 1);
}

static const char* SETTINGS_LABELS[ContactsSettingsModel::ROW_COUNT] = {
  "Auto-add all", "Auto-add Users", "Auto-add Repeaters", "Auto-add Rooms", "Auto-add Sensors",
  "Overwrite oldest", "Notify when full", "Max hops", "Remove non-users", "Remove non-favourites", "Remove all contacts",
};

bool ContactsSettingsModel::addAll() const {
  return _svc ? _svc->getAutoAdd().autoAddAll : true;
}

int ContactsSettingsModel::buildRows(Row* seq) const {
  int n = 0;
  seq[n++] = AutoAddAll;
  if (!addAll()) { seq[n++] = Users; seq[n++] = Repeaters; seq[n++] = Rooms; seq[n++] = Sensors; }
  seq[n++] = Overwrite;
  if (_svc && !_svc->getAutoAdd().overwriteOldest) seq[n++] = NotifyFull;
  seq[n++] = MaxHops;
  seq[n++] = RemoveNonUsers; seq[n++] = RemoveNonFavourites; seq[n++] = RemoveAll;
  return n;
}
ContactsSettingsModel::Row ContactsSettingsModel::rowAt(int i) const {
  Row seq[ROW_COUNT];
  int n = buildRows(seq);
  return (i >= 0 && i < n) ? seq[i] : ROW_COUNT;
}
int ContactsSettingsModel::count() const {
  Row seq[ROW_COUNT];
  return buildRows(seq);
}
const char* ContactsSettingsModel::label(int i) const {
  Row r = rowAt(i);
  return (r >= 0 && r < ROW_COUNT) ? SETTINGS_LABELS[r] : "";
}
bool ContactsSettingsModel::isToggle(int i) const {
  Row r = rowAt(i);
  return r <= Overwrite || r == NotifyFull;
}
bool ContactsSettingsModel::toggleState(int i) const {
  if (!_svc) return false;
  AutoAddConfig c = _svc->getAutoAdd();
  switch (rowAt(i)) {
    case AutoAddAll: return c.autoAddAll;
    case Users:      return c.addChat;
    case Repeaters:  return c.addRepeater;
    case Rooms:      return c.addRoom;
    case Sensors:    return c.addSensor;
    case Overwrite:  return c.overwriteOldest;
    case NotifyFull: return c.notifyWhenFull;
    default:         return false;
  }
}
const char* ContactsSettingsModel::value(int i) const {
  if (!_svc || rowAt(i) != MaxHops) return nullptr;
  static char buf[12];
  maxHopsLabel(_svc->getAutoAdd().maxHops, buf, sizeof(buf));
  return buf;
}

void ContactsSettingsPanel::begin(AppletContext& ctx) {
  _svc  = ctx.contacts;
  _host = ctx.host;
  _model.bind(_svc);
  _list.setRowHeight(14);
  _list.setModel(&_model);
  _list.resetSelection();   // singleton: force top-of-list on each open (setModel skips reset for same ptr)
  _editingHops = false; _confirming = false; _pendingAction = -1;
}

int ContactsSettingsPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  _list.draw(c, x, y, w, h);
  if (_confirming)  { _confirm.draw(c, 0, 0, c.width(), c.height()); return 100; }
  if (_editingHops) { _hops.draw(c, 0, 0, c.width(), c.height());    return 100; }
  return _list.needsAnimation() ? ListMenu::TICK_MS : 500;
}

bool ContactsSettingsPanel::onInput(InputEvent ev) {
  if (_editingHops) {
    if (_hops.onInput(ev)) {
      StepperResult r = _hops.result();
      if (r != StepperResult::None) {
        if (r == StepperResult::Confirmed && _svc) {
          AutoAddConfig cfg = _svc->getAutoAdd();
          cfg.maxHops = (uint8_t)_hops.value();
          _svc->setAutoAdd(cfg);
        }
        _editingHops = false;
        _hops.reset();
      }
    }
    return true;
  }

  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _svc) {
          int removed = -1;
          if (_pendingAction == ContactsSettingsModel::RemoveNonUsers)            removed = _svc->removeNonChat();
          else if (_pendingAction == ContactsSettingsModel::RemoveNonFavourites)  removed = _svc->removeNonFavourites();
          else if (_pendingAction == ContactsSettingsModel::RemoveAll)            removed = _svc->removeAll();
          if (removed >= 0 && _host) {
            char buf[20]; snprintf(buf, sizeof(buf), "Removed %d", removed);
            _host->postToast(buf);
          }
        }
        _confirming = false; _pendingAction = -1;
      }
      return true;
    }
    return true;
  }

  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    int i = _list.selected();
    ContactsSettingsModel::Row r = _model.rowAt(i);
    if (_model.isToggle(i) && _svc) {
      AutoAddConfig cfg = _svc->getAutoAdd();
      switch (r) {
        case ContactsSettingsModel::AutoAddAll: cfg.autoAddAll = !cfg.autoAddAll; break;
        case ContactsSettingsModel::Users:      cfg.addChat = !cfg.addChat; break;
        case ContactsSettingsModel::Repeaters:  cfg.addRepeater = !cfg.addRepeater; break;
        case ContactsSettingsModel::Rooms:      cfg.addRoom = !cfg.addRoom; break;
        case ContactsSettingsModel::Sensors:    cfg.addSensor = !cfg.addSensor; break;
        case ContactsSettingsModel::Overwrite:  cfg.overwriteOldest = !cfg.overwriteOldest; break;
        case ContactsSettingsModel::NotifyFull: cfg.notifyWhenFull = !cfg.notifyWhenFull; break;
        default: break;
      }
      _svc->setAutoAdd(cfg);
    } else if (r == ContactsSettingsModel::MaxHops && _svc) {
      _hops.configure("Max hops", _svc->getAutoAdd().maxHops, 0, 64, maxHopsLabel);
      _editingHops = true;
    } else {
      _pendingAction = r;
      const char* msg = "Remove non-user contacts?";
      if (r == ContactsSettingsModel::RemoveAll) msg = "Remove ALL contacts?";
      else if (r == ContactsSettingsModel::RemoveNonFavourites) msg = "Remove all non-favourites?";
      _confirm.configure(msg);
      _confirming = true;
    }
    return true;
  }
  return false;   // Back bubbles
}

ContactsSettingsPanel& contactsSettings() {
  static ContactsSettingsPanel s;
  return s;
}

}  // namespace mishmesh
