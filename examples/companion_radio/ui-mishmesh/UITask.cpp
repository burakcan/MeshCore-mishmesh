#include "UITask.h"
#include "target.h"   // rtc_clock
#include <mishmesh/core/TelemetryDecode.h>
#include <helpers/AdvertDataHelpers.h>   // ADV_TYPE_CHAT

uint32_t UITask::epochSeconds() const {
  return rtc_clock.getCurrentTime();
}

uint16_t UITask::batteryMillivolts() const {
  uint32_t now = millis();
  if (_batt_mv == 0 || now - _batt_sampled_at >= 8000) {
    uint16_t mv = getBattMilliVolts();
    _batt_mv = _batt_mv ? (uint16_t)((_batt_mv * 3 + mv) / 4) : mv;
    _batt_sampled_at = now;
  }
  return _batt_mv;
}

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _node_prefs = node_prefs;

  if (_display == nullptr) return;   // headless build

  mishmesh::AppletContext ctx;
  ctx.app = this;
  ctx.contacts = this;
  _host = new mishmesh::AppletHost(_display, ctx);

  _menu = new mishmesh::AppMenuApplet();
  _home = new mishmesh::HomeApplet();
  _home->setMenu(_menu);
  _host->setRoot(_home);

#ifdef UI_HAS_JOYSTICK
  // Wio Tracker L1 buttons are active-low (pull-up), hence reverse=true.
  mishmesh::DirectionalMap dirMap;
  _joystick = new mishmesh::DirectionalSource(
      JOYSTICK_UP, JOYSTICK_DOWN, JOYSTICK_LEFT, JOYSTICK_RIGHT, JOYSTICK_PRESS, dirMap, 1000, /*reverse*/true);
  _joystick->begin();
  _host->addSource(_joystick);

  mishmesh::GestureMap backMap;
  backMap.click = mishmesh::InputEvent::Back;
  backMap.longPress = mishmesh::InputEvent::BackLong;
  _backBtn = new mishmesh::ButtonGestureSource(PIN_BACK_BTN, backMap, 1000, /*reverse*/true);
  _backBtn->begin();
  _host->addSource(_backBtn);
#else
  mishmesh::GestureMap userMap;
  userMap.click = mishmesh::InputEvent::NavDown;
  userMap.doubleClick = mishmesh::InputEvent::Select;
  userMap.longPress = mishmesh::InputEvent::Back;
  _userBtn = new mishmesh::ButtonGestureSource(PIN_USER_BTN, userMap, 1000, /*reverse*/true);
  _userBtn->begin();
  _host->addSource(_userBtn);
#endif
}

void UITask::msgRead(int /*msgcount*/) {
}

void UITask::newMsg(uint8_t /*path_len*/, const char* /*from_name*/,
                    const char* /*text*/, int /*msgcount*/) {
}

void UITask::notify(UIEventType /*t*/) {
}

void UITask::loop() {
  if (_host != nullptr) {
    _host->loop(millis());
  }
}

// --- mishmesh::ContactsService -------------------------------------------------
// AUTO_ADD_* bitmask values come from NodePrefs.h (shared with MyMesh).

int UITask::countByKind(mishmesh::ContactKind k) const {
  int n = the_mesh.getNumContacts();
  int count = 0;
  for (int i = 0; i < n; i++) {
    if (the_mesh.getContactByIdx(i, _scratch) && _scratch.type == (uint8_t)k) count++;
  }
  return count;
}

void UITask::fillView(const ContactInfo& c, mishmesh::ContactView& out) {
  out.name = c.name;
  out.type = c.type;
  out.isFavourite = (c.flags & 0x01) != 0;
  out.hasPath = c.out_path_len != OUT_PATH_UNKNOWN;
  out.hops = out.hasPath ? c.out_path_len : 0;
  out.lastAdvert = c.last_advert_timestamp;
  out.pubKey = c.id.pub_key;
  out.hasLocation = (c.gps_lat != 0 || c.gps_lon != 0);
  out.gpsLat = c.gps_lat;
  out.gpsLon = c.gps_lon;
}

bool UITask::getByKind(mishmesh::ContactKind k, int index, mishmesh::ContactView& out) const {
  int n = the_mesh.getNumContacts();
  int seen = 0;
  for (int i = 0; i < n; i++) {
    if (the_mesh.getContactByIdx(i, _scratch) && _scratch.type == (uint8_t)k) {
      if (seen == index) { fillView(_scratch, out); return true; }
      seen++;
    }
  }
  return false;
}

int UITask::countFavourites() const {
  int n = the_mesh.getNumContacts();
  int count = 0;
  for (int i = 0; i < n; i++)
    if (the_mesh.getContactByIdx(i, _scratch) && (_scratch.flags & 0x01)) count++;
  return count;
}

bool UITask::getFavourite(int index, mishmesh::ContactView& out) const {
  int n = the_mesh.getNumContacts();
  int seen = 0;
  for (int i = 0; i < n; i++) {
    if (the_mesh.getContactByIdx(i, _scratch) && (_scratch.flags & 0x01)) {
      if (seen == index) { fillView(_scratch, out); return true; }
      seen++;
    }
  }
  return false;
}

bool UITask::setFavourite(const uint8_t* pk, bool fav) { return the_mesh.uiSetFavourite(pk, fav); }

bool UITask::selfLocation(int32_t& lat, int32_t& lon) const {
  if (!_sensors) return false;
  lat = (int32_t)(_sensors->node_lat * 1e6);
  lon = (int32_t)(_sensors->node_lon * 1e6);
  return lat != 0 || lon != 0;
}

bool UITask::requestTelemetry(const uint8_t* pk) { return the_mesh.uiRequestTelemetry(pk); }
bool UITask::resetPath(const uint8_t* pk) {
  ContactInfo* c = the_mesh.lookupContactByPubKey(pk, 6);
  if (!c) return false;
  the_mesh.resetPathTo(*c);
  the_mesh.uiPersistContacts();
  return true;
}
bool UITask::clearConversation(const uint8_t* pk) { return the_mesh.uiClearConversation(pk); }
bool UITask::deleteContact(const uint8_t* pk) { return the_mesh.uiDeleteContact(pk); }

uint32_t UITask::telemetrySeq() const { return the_mesh.uiLastTelemetry().seq; }
bool UITask::latestTelemetry(const uint8_t* pk, mishmesh::TelemetryView& out) const {
  const MyMesh::TelemetryLatch& l = the_mesh.uiLastTelemetry();
  if (memcmp(l.pubkey, pk, 6) != 0 || l.len == 0) { out.valid = false; out.count = 0; return false; }
  mishmesh::decodeTelemetry(l.lpp, l.len, out);
  return out.valid;
}

bool UITask::ping(const uint8_t* pk) { return the_mesh.uiPing(pk); }
uint32_t UITask::pingSeq() const { return the_mesh.uiLastPing().seq; }
bool UITask::latestPing(const uint8_t* pk, mishmesh::PingView& out) const {
  const MyMesh::PingLatch& l = the_mesh.uiLastPing();
  if (memcmp(l.pubkey, pk, 6) != 0) { out.replied = false; out.rttMs = 0; out.hops = 0; out.snrUs = 0; out.snrThem = 0; return false; }
  out.replied = l.replied; out.rttMs = l.rttMs; out.hops = l.hops; out.snrUs = l.snrUs; out.snrThem = l.snrThem;
  return true;
}

mishmesh::AutoAddConfig UITask::getAutoAdd() const {
  NodePrefs* p = the_mesh.getNodePrefs();
  mishmesh::AutoAddConfig c;
  c.addChat     = (p->autoadd_config & AUTO_ADD_CHAT) != 0;
  c.addRepeater = (p->autoadd_config & AUTO_ADD_REPEATER) != 0;
  c.addRoom     = (p->autoadd_config & AUTO_ADD_ROOM_SERVER) != 0;
  c.addSensor   = (p->autoadd_config & AUTO_ADD_SENSOR) != 0;
  c.overwriteOldest = (p->autoadd_config & AUTO_ADD_OVERWRITE_OLDEST) != 0;
  c.maxHops = p->autoadd_max_hops;
  return c;
}
void UITask::setAutoAdd(const mishmesh::AutoAddConfig& c) {
  NodePrefs* p = the_mesh.getNodePrefs();
  uint8_t v = 0;
  if (c.addChat) v |= AUTO_ADD_CHAT;
  if (c.addRepeater) v |= AUTO_ADD_REPEATER;
  if (c.addRoom) v |= AUTO_ADD_ROOM_SERVER;
  if (c.addSensor) v |= AUTO_ADD_SENSOR;
  if (c.overwriteOldest) v |= AUTO_ADD_OVERWRITE_OLDEST;
  p->autoadd_config = v;
  p->autoadd_max_hops = c.maxHops;
  the_mesh.savePrefs();
}

int UITask::removeNonChat() {
  // Unbounded: re-scan from index 0 each time (removeContact compacts the array,
  // so a fixed snapshot would silently cap at its size - MAX_CONTACTS is 350 here).
  int removed = 0;
  bool found = true;
  while (found) {
    found = false;
    int n = the_mesh.getNumContacts();
    for (int i = 0; i < n; i++) {
      if (the_mesh.getContactByIdx(i, _scratch) && _scratch.type != ADV_TYPE_CHAT) {
        ContactInfo* c = the_mesh.lookupContactByPubKey(_scratch.id.pub_key, 6);
        if (c && the_mesh.removeContact(*c)) { removed++; found = true; }
        break;  // array shifted; restart scan
      }
    }
  }
  if (removed) the_mesh.uiPersistContacts();
  return removed;
}
int UITask::removeAll() {
  int removed = 0;
  ContactInfo tmp;
  while (the_mesh.getNumContacts() > 0 && the_mesh.getContactByIdx(0, tmp)) {
    ContactInfo* c = the_mesh.lookupContactByPubKey(tmp.id.pub_key, 6);
    if (c && the_mesh.removeContact(*c)) removed++; else break;
  }
  if (removed) the_mesh.uiPersistContacts();
  return removed;
}
