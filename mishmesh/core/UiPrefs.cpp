#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/AppletRegistry.h>
#include <string.h>

namespace mishmesh {

static const char* const QA_KEY[2] = { "qa_l", "qa_r" };
static const char* const QA_DEFAULT[2] = { "Contacts", "Messages" };

void UiPrefs::begin(AppletStorage* s) {
  _st = s;
  _battPercent = false;
  _dark = true;
  _qa[0][0] = _qa[1][0] = 0;
  if (!_st) return;
  uint8_t b = 0;
  if (_st->load("uibatt", &b, 1)) _battPercent = b != 0;
  if (_st->load("uithm", &b, 1)) _dark = b != 0;
  for (int i = 0; i < 2; i++) {
    uint8_t n = _st->load(QA_KEY[i], (uint8_t*)_qa[i], LABEL_CAP - 1);
    _qa[i][n] = 0;
  }
}

void UiPrefs::setBattShowPercent(bool on) {
  _battPercent = on;
  if (_st) { uint8_t b = on ? 1 : 0; _st->save("uibatt", &b, 1); }
}

void UiPrefs::setDarkMode(bool on) {
  _dark = on;
  if (_st) { uint8_t b = on ? 1 : 0; _st->save("uithm", &b, 1); }
}

const char* UiPrefs::quickActionLabel(int slot) const {
  if (slot < 0 || slot > 1) return "";
  return _qa[slot][0] ? _qa[slot] : QA_DEFAULT[slot];
}

void UiPrefs::setQuickAction(int slot, const char* label) {
  if (slot < 0 || slot > 1 || !label) return;
  size_t n = strlen(label);
  if (n >= LABEL_CAP) n = LABEL_CAP - 1;
  memcpy(_qa[slot], label, n);
  _qa[slot][n] = 0;
  if (_st && n) _st->save(QA_KEY[slot], (const uint8_t*)_qa[slot], (uint8_t)n);
}

static const AppletRegistration* findMenuApplet(const char* label) {
  for (const AppletRegistration* r = registeredApplets(); r; r = r->next) {
    if (r->placement == Placement::AppMenu && strcmp(r->label, label) == 0)
      return r;
  }
  return nullptr;
}

const AppletRegistration* UiPrefs::quickAction(int slot) const {
  if (slot < 0 || slot > 1) return nullptr;
  if (_qa[slot][0]) {
    const AppletRegistration* r = findMenuApplet(_qa[slot]);
    if (r) return r;
  }
  return findMenuApplet(QA_DEFAULT[slot]);
}

void UiPrefs::resetForTest() {
  _st = nullptr;
  _battPercent = false;
  _dark = true;
  _qa[0][0] = _qa[1][0] = 0;
}

UiPrefs& uiPrefs() {
  static UiPrefs s;
  return s;
}

}  // namespace mishmesh
