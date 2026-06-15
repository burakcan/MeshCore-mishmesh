// mishmesh/widgets/ChatMenu.cpp
#include <mishmesh/widgets/ChatMenu.h>
#include <stdio.h>

namespace mishmesh {

// Notify stepper <-> level mapping. Channels use the full enum as the step value
// (0=All, 1=MentionsOnly, 2=Mute); DMs offer only {All, Mute} as a 2-step range
// (0=All, 1=Mute), since "Mentions only" is meaningless on a 1:1 chat.
static NotifyLevel levelFromStep(bool channel, int v) {
  if (channel) return (NotifyLevel)v;
  return v == 0 ? NotifyLevel::All : NotifyLevel::Mute;
}
static int stepFromLevel(bool channel, NotifyLevel l) {
  if (channel) return (int)l;
  return l == NotifyLevel::Mute ? 1 : 0;
}
static void fmtChannelLevel(int v, char* out, uint16_t cap) {
  snprintf(out, cap, "%s", v == 0 ? "All" : v == 1 ? "Mentions only" : "Mute");
}
static void fmtDmLevel(int v, char* out, uint16_t cap) {
  snprintf(out, cap, "%s", v == 0 ? "All" : "Mute");
}

ChatMenu::Result ChatMenu::activate(MessagesService* svc, const char*& toast) {
  toast = nullptr;
  _svc = svc;
  if (!svc) return Result::None;
  switch (_menu.selected()) {
    case 0: return Result::EditRegion;   // caller opens the region editor (keypad)
    case 1: {                            // Notifications -> value stepper (in place)
      bool ch = (_key.type == 1);
      _stepper.configure("Notifications", stepFromLevel(ch, svc->notifyLevel(_key)),
                         0, ch ? 2 : 1, ch ? fmtChannelLevel : fmtDmLevel);
      _stepping = true;
      return Result::None;
    }
    case 3: svc->markUnread(_key); toast = "Marked unread"; return Result::None;
    case 2: _confirm.configure("Clear this chat?");   _confirming = true; return Result::None;
    default: _confirm.configure("Delete this chat?"); _confirming = true; return Result::None;
  }
}

bool ChatMenu::onInput(InputEvent ev) {
  if (_stepping) {
    if (_stepper.onInput(ev)) {
      StepperResult r = _stepper.result();
      if (r == StepperResult::Confirmed) {
        NotifyLevel lvl = levelFromStep(_key.type == 1, _stepper.value());
        if (_svc) _svc->setNotifyLevel(_key, lvl);
        setNotifyLabel(notifyLevelShortLabel(lvl));   // reflect the new value in the row
        _stepping = false; _stepper.reset();
      } else if (r == StepperResult::Cancelled) {
        _stepping = false; _stepper.reset();
      }
    }
    return true;   // swallow all input while the stepper is up
  }
  if (!_confirming) return _menu.onInput(ev);

  if (_confirm.onInput(ev)) {
    ConfirmResult r = _confirm.result();
    if (r == ConfirmResult::Confirmed) {
      if (_menu.selected() == 2) {        // Clear chat
        if (_svc) _svc->clearConvo(_key);
        _toast = "Chat cleared"; _pending = Result::Cleared;
      } else {                            // Delete chat
        if (_svc) _svc->deleteConvo(_key);
        _toast = "Chat deleted"; _pending = Result::Deleted;
      }
      _confirming = false; _confirm.reset();
    } else if (r == ConfirmResult::Cancelled) {
      _confirming = false; _confirm.reset();   // back to the action list, nothing done
    }
  }
  return true;   // swallow all input while the dialog is up
}

}  // namespace mishmesh
