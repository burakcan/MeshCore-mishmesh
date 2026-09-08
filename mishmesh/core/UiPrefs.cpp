#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/AppletRegistry.h>
#include <string.h>

namespace mishmesh {

static const char* const QA_KEY[2] = { "qa_l", "qa_r" };
static const char* const QA_DEFAULT[2] = { "Contacts", "Messages" };

static int clampCal(int pct) { return pct < 50 ? 50 : pct > 150 ? 150 : pct; }

void UiPrefs::begin(AppletStorage* s, bool defaultDark) {
  _st = s;
  _battMode = BattMode::Gauge;
  _battCal = 100;
  _dark = defaultDark;
  _uiScale = 2;
  _rotation = 0;
  _inputRot = INPUT_AUTO;
  _qa[0][0] = _qa[1][0] = 0;
  if (!_st) return;
  uint8_t b = 0;
  if (_st->load("uibatt", &b, 1)) _battMode = b <= 2 ? (BattMode)b : BattMode::Gauge;
  if (_st->load("battcal", &b, 1)) _battCal = clampCal(b);
  if (_st->load("uithm", &b, 1)) _dark = b != 0;
  if (_st->load("uiscl", &b, 1)) _uiScale = (b >= 1 && b <= 4) ? b : 2;
  if (_st->load("uirot", &b, 1)) _rotation = b < ROTATIONS ? b : 0;
  if (_st->load("uiinr", &b, 1)) _inputRot = (b == 0) ? INPUT_AUTO : (int)(b - 1) % ROTATIONS;
  for (int i = 0; i < 2; i++) {
    uint8_t n = _st->load(QA_KEY[i], (uint8_t*)_qa[i], LABEL_CAP - 1);
    _qa[i][n] = 0;
  }
}

void UiPrefs::setBattMode(BattMode m) {
  _battMode = m;
  if (_st) { uint8_t b = (uint8_t)m; _st->save("uibatt", &b, 1); }
}

void UiPrefs::setBattCalPercent(int pct) {
  _battCal = clampCal(pct);
  if (_st) { uint8_t b = (uint8_t)_battCal; _st->save("battcal", &b, 1); }
}

void UiPrefs::setUiScale(int mult) {
  if (mult < 1) mult = 1;
  if (mult > 4) mult = 4;
  _uiScale = mult;
  if (_st) { uint8_t b = (uint8_t)_uiScale; _st->save("uiscl", &b, 1); }
}

void UiPrefs::setRotation(int quarters) {
  _rotation = ((quarters % ROTATIONS) + ROTATIONS) % ROTATIONS;
  if (_st) { uint8_t b = (uint8_t)_rotation; _st->save("uirot", &b, 1); }
}

void UiPrefs::setInputRotation(int quartersOrAuto) {
  _inputRot = quartersOrAuto == INPUT_AUTO
                ? INPUT_AUTO
                : ((quartersOrAuto % ROTATIONS) + ROTATIONS) % ROTATIONS;
  // Stored with a +1 bias so 0 can mean "follow the display" on a fresh device.
  if (_st) {
    uint8_t b = _inputRot == INPUT_AUTO ? 0 : (uint8_t)(_inputRot + 1);
    _st->save("uiinr", &b, 1);
  }
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
  _battMode = BattMode::Gauge;
  _battCal = 100;
  _dark = true;
  _qa[0][0] = _qa[1][0] = 0;
}

UiPrefs& uiPrefs() {
  static UiPrefs s;
  return s;
}

}  // namespace mishmesh
