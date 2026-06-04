#include "UITask.h"
#include "target.h"   // rtc_clock
#include <mishmesh/core/TelemetryDecode.h>
#include <helpers/AdvertDataHelpers.h>   // ADV_TYPE_CHAT
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <malloc.h>
#endif
#if defined(NRF52_PLATFORM)
  // Linker symbols bounding the heap. On the Adafruit nRF52 core new/malloc share
  // one newlib arena grown via _sbrk, so the real free RAM is the headroom above
  // the break plus freed chunks, i.e. total - currently-allocated. mallinfo()'s
  // fordblks alone misses the un-sbrk'd headroom and reads ~0.
  extern "C" {
    extern unsigned char __HeapBase[];
    extern unsigned char __HeapLimit[];
  }
#endif

// [mishmesh] shared load/save scratch (definition of the class-level static)
uint8_t UITask::_msgIoBuf[mishmesh::ARENA_BYTES + 2048];
// [/mishmesh]

// Free and total heap in bytes, best-effort per platform; 0 where unavailable.
static void platformHeap(uint32_t& freeBytes, uint32_t& totalBytes) {
#if defined(ESP32)
  freeBytes  = ESP.getFreeHeap();
  totalBytes = ESP.getHeapSize();
#elif defined(RP2040_PLATFORM)
  freeBytes  = rp2040.getFreeHeap();
  totalBytes = rp2040.getTotalHeap();
#elif defined(NRF52_PLATFORM)
  struct mallinfo mi = mallinfo();
  totalBytes = (uint32_t)(__HeapLimit - __HeapBase);
  uint32_t used = (uint32_t)mi.uordblks;
  freeBytes = used <= totalBytes ? totalBytes - used : 0;
#elif defined(STM32_PLATFORM)
  struct mallinfo mi = mallinfo();
  freeBytes  = (uint32_t)mi.fordblks;   // best-effort; total unknown
  totalBytes = 0;
#else
  freeBytes  = 0;
  totalBytes = 0;
#endif
}

uint32_t UITask::epochSeconds() const {
  return rtc_clock.getCurrentTime();
}

bool UITask::systemStats(mishmesh::SystemStats& out) const {
  uint32_t freeHeap = 0, totalHeap = 0;
  platformHeap(freeHeap, totalHeap);
  if (freeHeap && (_heap_min == 0 || freeHeap < _heap_min)) _heap_min = freeHeap;

  out.heapFreeBytes    = freeHeap;
  out.heapMinFreeBytes = _heap_min;
  out.heapTotalBytes   = totalHeap;
  out.contactsUsed     = (uint16_t)the_mesh.getNumContacts();
  out.contactsMax      = (uint16_t)MAX_CONTACTS;
  out.storageUsedKb    = the_mesh.getStorageUsedKb();
  out.storageTotalKb   = the_mesh.getStorageTotalKb();
  out.uptimeSecs       = millis() / 1000;
  out.batteryMv        = batteryMillivolts();
#ifdef FIRMWARE_VERSION
  out.firmwareVersion  = FIRMWARE_VERSION;
#else
  out.firmwareVersion  = nullptr;
#endif
  return true;
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

  // [mishmesh] load persisted messages before wiring the store so that any
  // inbound messages captured after uiSetMessageStore append to loaded history.
  {
    size_t n = 0;
    DataStore* ds = the_mesh.getStore();
    if (ds && ds->loadMessages(_msgIoBuf, sizeof(_msgIoBuf), n)) _msgStore.deserialize(_msgIoBuf, n);
  }
  _msgSvc.store = &_msgStore;
  the_mesh.uiSetMessageStore(&_msgStore);
  // Surface joined channels (e.g. the default Public channel) as chats even
  // before any message arrives - the store is otherwise only fed on message capture.
  the_mesh.uiSeedChannels();
  // [/mishmesh]

  mishmesh::AppletContext ctx;
  ctx.app = this;
  ctx.contacts = this;
  // [mishmesh]
  ctx.messages = &_msgSvc;
  // [/mishmesh]
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
  // [mishmesh] store capture already happened in MyMesh::queueMessage; just schedule
  // a flush. Fired from inside a mesh recv callback, so don't drive the render
  // pipeline here - the next UITask::loop() repaints naturally.
  _msgFlushAt = millis() + 8000;
  // [/mishmesh]
}

void UITask::notify(UIEventType /*t*/) {
}

void UITask::loop() {
  // [mishmesh] debounced message-store persistence
  if (_msgStore.seq() != _msgDirtySeq) {
    _msgDirtySeq = _msgStore.seq();
    _msgFlushAt  = millis() + 8000;
  }
  if (_msgFlushAt && millis() >= _msgFlushAt) {
    _msgFlushAt = 0;
    DataStore* ds = the_mesh.getStore();
    if (ds) {
      size_t n = _msgStore.serialize(_msgIoBuf, sizeof(_msgIoBuf));
      if (n) ds->saveMessages(_msgIoBuf, n);
    }
  }
  // [/mishmesh]
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
bool UITask::renameContact(const uint8_t* pk, const char* name) { return the_mesh.uiRenameContact(pk, name); }

int UITask::countDiscovered() const { return the_mesh.uiDiscoveryCount(); }
bool UITask::getDiscovered(int index, mishmesh::ContactView& out) const {
  if (!the_mesh.uiGetDiscovery(index, _scratch)) return false;
  fillView(_scratch, out);
  return true;
}
bool UITask::addDiscovered(const uint8_t* pk) { return the_mesh.uiAddDiscovery(pk); }

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
  // manual_add_contacts bit0: 0 = auto-add everything, 1 = only the selected kinds.
  c.autoAddAll  = (p->manual_add_contacts & 1) == 0;
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
  p->manual_add_contacts = c.autoAddAll ? 0 : 1;   // bit0: 0 = add all, 1 = selected kinds
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
int UITask::removeNonFavourites() {
  // Same unbounded rescan as removeNonChat, but keeps only favourites (flags bit0).
  int removed = 0;
  bool found = true;
  while (found) {
    found = false;
    int n = the_mesh.getNumContacts();
    for (int i = 0; i < n; i++) {
      if (the_mesh.getContactByIdx(i, _scratch) && (_scratch.flags & 0x01) == 0) {
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

// [mishmesh] --- MessagesService implementation ---

const char* UITask::MsgSvc::nameFor(const mishmesh::ConvoKey& k) const {
  static char buf[34];
  if (k.type == 1) {
    ChannelDetails cd;
    if (the_mesh.getChannel(k.id[0], cd)) { snprintf(buf, sizeof(buf), "%s", cd.name); return buf; }
    snprintf(buf, sizeof(buf), "#%u", (unsigned)k.id[0]); return buf;
  }
  ContactInfo* c = the_mesh.lookupContactByPubKey(k.id, 6);
  if (c) { snprintf(buf, sizeof(buf), "%s", c->name); return buf; }
  snprintf(buf, sizeof(buf), "%02X%02X", k.id[0], k.id[1]); return buf;
}

int UITask::MsgSvc::convoCount() const { return store->convoCount(); }

bool UITask::MsgSvc::getConvo(int i, mishmesh::ConvoView& out) const {
  mishmesh::ConvoSummary c;
  if (!store->getConvo(i, c)) return false;
  out.key       = c.key;
  out.isChannel = c.key.type == 1;
  out.name      = nameFor(c.key);
  out.lastTime  = c.lastTime;
  out.unread    = c.unread;
  // preview: last message text, null-terminated, truncated to 47 chars
  static char prevBuf[48];
  prevBuf[0] = '\0';
  int cnt = store->messageCount(c.key);
  if (cnt > 0) {
    mishmesh::MsgRecord r;
    if (store->getMessage(c.key, cnt - 1, r)) {
      uint16_t n = r.textLen < (uint16_t)(sizeof(prevBuf) - 1) ? r.textLen : (uint16_t)(sizeof(prevBuf) - 1);
      memcpy(prevBuf, r.text, n);
      prevBuf[n] = '\0';
    }
  }
  out.preview = prevBuf;
  return true;
}

uint16_t UITask::MsgSvc::totalUnread() const { return store->totalUnread(); }
int  UITask::MsgSvc::messageCount(const mishmesh::ConvoKey& k) const { return store->messageCount(k); }

bool UITask::MsgSvc::getMessage(const mishmesh::ConvoKey& k, int i, mishmesh::MessageView& out) const {
  mishmesh::MsgRecord r;
  if (!store->getMessage(k, i, r)) return false;
  out.outbound    = r.kind != mishmesh::KIND_INBOUND;
  out.isChannel   = k.type == 1;
  out.kind        = r.kind;
  out.senderTime  = r.senderTime;
  out.localTime   = r.localTime;
  out.status      = r.status;
  out.tripTimeMs  = r.tripTimeMs;
  out.heardCount  = r.heardCount;
  out.snrx4       = r.snrx4;
  out.hops        = r.hops;
  out.path        = r.path;
  out.pathLen     = r.pathLen;

  // copy text to null-terminated buffer; for inbound channel split "name: body"
  static char textBuf[mishmesh::MAX_TEXT + 1];
  static char senderBuf[34];
  uint16_t n = r.textLen < mishmesh::MAX_TEXT ? r.textLen : (uint16_t)mishmesh::MAX_TEXT;
  memcpy(textBuf, r.text, n);
  textBuf[n] = '\0';
  out.senderName = "";
  out.text       = textBuf;
  if (k.type == 1 && r.kind == mishmesh::KIND_INBOUND) {
    char* sep = strstr(textBuf, ": ");
    if (sep) {
      *sep = '\0';
      snprintf(senderBuf, sizeof(senderBuf), "%s", textBuf);
      out.senderName = senderBuf;
      out.text       = sep + 2;
    }
  }
  return true;
}

void UITask::MsgSvc::setActiveConvo(const mishmesh::ConvoKey& k) { store->setActiveConvo(k); }
void UITask::MsgSvc::clearActiveConvo() { store->clearActiveConvo(); }
int  UITask::MsgSvc::repeatCount(const mishmesh::ConvoKey& k, int m) const { return store->repeatCount(k, m); }

bool UITask::MsgSvc::getRepeat(const mishmesh::ConvoKey& k, int m, int r, mishmesh::RepeatView& out) const {
  mishmesh::RepeatRec rr;
  if (!store->getRepeat(k, m, r, rr)) return false;
  out.hops  = rr.hops;
  out.snrx4 = rr.snrx4;
  const char* rname; uint8_t kc;
  if (rr.pathLen > 0 && resolveHop(rr.path[rr.pathLen - 1], rname, kc)) {
    out.repeaterName = rname;
    out.knownCount   = kc;
  } else {
    static char hexBuf[3];
    if (rr.pathLen > 0) { snprintf(hexBuf, sizeof(hexBuf), "%02X", rr.path[rr.pathLen - 1]); out.repeaterName = hexBuf; }
    else out.repeaterName = "";
    out.knownCount = 0;
  }
  return true;
}

bool UITask::MsgSvc::resolveHop(uint8_t hashByte, const char*& name, uint8_t& knownCount) const {
  static char buf[34]; knownCount = 0; name = "";
  int n = the_mesh.getNumContacts();
  bool hasFirst = false;
  ContactInfo ci;
  for (int i = 0; i < n; i++) {
    if (!the_mesh.getContactByIdx(i, ci)) continue;
    if (ci.id.pub_key[0] == hashByte) {
      if (!hasFirst) { snprintf(buf, sizeof(buf), "%s", ci.name); hasFirst = true; }
      knownCount++;
    }
  }
  name = hasFirst ? buf : "";
  return hasFirst;
}

void UITask::MsgSvc::deleteMessage(const mishmesh::ConvoKey& k, int i) { store->deleteMessage(k, i); }
void UITask::MsgSvc::clearConvo(const mishmesh::ConvoKey& k) { store->clearConvo(k); }
void UITask::MsgSvc::deleteConvo(const mishmesh::ConvoKey& k) { store->deleteConvo(k); }
void UITask::MsgSvc::markUnread(const mishmesh::ConvoKey& k) { store->markUnread(k); }
uint32_t UITask::MsgSvc::seq() const { return store->seq(); }

bool UITask::MsgSvc::sendText(const mishmesh::ConvoKey& k, const char* text) {
  return the_mesh.mishmeshSendText(k, text);
}

// [/mishmesh]
