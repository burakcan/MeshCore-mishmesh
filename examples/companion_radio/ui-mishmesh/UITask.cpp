#include "UITask.h"
#include "target.h"   // rtc_clock
// [mishmesh]
#include <helpers/sensors/LocationProvider.h>
// [/mishmesh]
#include <mishmesh/applets/NotificationApplet.h>
#include <mishmesh/applets/ClockAlertApplet.h>
#include <mishmesh/core/ClockService.h>
#include <mishmesh/core/QuickReplyStore.h>
#include <mishmesh/core/UiPrefs.h>
#include <mishmesh/core/TelemetryDecode.h>
#include <mishmesh/core/Mention.h>
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

// [mishmesh] (ExtraFsMsgBackend replaces the old _msgIoBuf snapshot approach)
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

// [mishmesh]
void UITask::setAutoTimeSync(bool on) {
  NodePrefs* p = the_mesh.getNodePrefs();
  if (p) { p->manual_time_set = on ? 0 : 1; the_mesh.savePrefs(); }
  applyTimeSyncGate(on);
  // Turning auto back on: request a prompt re-sync so the clock corrects on the
  // next GPS fix instead of waiting out the ~30-min interval. Suppression-only:
  // this arms a pending sync; it does not power GPS on, so it's a no-op when GPS
  // is disabled or has no fix.
  if (on && _sensors) {
    LocationProvider* lp = _sensors->getLocationProvider();
    if (lp) lp->syncTime();
  }
}

void UITask::setEpochSeconds(uint32_t secs) {
  rtc_clock.setCurrentTime(secs);
}

void UITask::applyTimeSyncGate(bool on) {
  if (!_sensors) return;
  LocationProvider* lp = _sensors->getLocationProvider();
  if (lp) lp->setTimeSyncEnabled(on);
}
// [/mishmesh]

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

bool UITask::airtimeStats(mishmesh::AirtimeStats& out) const {
  out.txTotalMs  = the_mesh.getTotalAirTime();
  out.rxTotalMs  = the_mesh.getReceiveAirTime();
  out.txBudgetMs = the_mesh.getRemainingTxBudget();
  // Budget capacity mirrors Dispatcher's math: window * 1/(1 + airtime_factor).
  // getDutyCycleWindowMs()/getAirtimeBudgetFactor() aren't public, so recompute
  // from the same inputs - the window is the Dispatcher default (1h), and the
  // factor is the persisted NodePrefs value MyMesh feeds back to the Dispatcher.
  const uint32_t windowMs = 3600000u;
  float factor = _node_prefs ? _node_prefs->airtime_factor : 1.0f;
  if (factor < 0.0f) factor = 0.0f;
  out.windowMs    = windowMs;
  out.txBudgetMax = (uint32_t)(windowMs / (1.0f + factor));
  out.sentFlood   = the_mesh.getNumSentFlood();
  out.sentDirect  = the_mesh.getNumSentDirect();
  out.recvFlood   = the_mesh.getNumRecvFlood();
  out.recvDirect  = the_mesh.getNumRecvDirect();
  out.history     = &_airtime;
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
  // [mishmesh] apply the persisted auto/manual time-sync state to the GPS provider
  applyTimeSyncGate(_node_prefs ? _node_prefs->manual_time_set == 0 : true);
  // Re-apply the persisted BLE/serial toggle: startInterface() unconditionally
  // enable()s the link on every boot, so honor a stored "off" here.
  if (_node_prefs && _node_prefs->ble_enabled == 0) disableSerial();
  // [/mishmesh]

  if (_display == nullptr) return;   // headless build

  // [mishmesh] wire the flash backend and load persisted index/history.
  {
    DataStore* ds = the_mesh.getStore();
    if (ds && ds->extraFs()) _backend.begin(*ds->extraFs());
  }
  _msgStore.begin(&_backend);
  _msgSvc.store = &_msgStore;
  _msgSvc.retry = &_retry;            // expose live retry counts to the thread view
  _retryGlue.store = &_msgStore;      // auto-retry acts on the same store
  _theStorage.ds = the_mesh.getStore();
  _msgSvc.storage = &_theStorage;     // per-chat region persistence
  the_mesh.uiSetMessageStore(&_msgStore);
  // Surface joined channels (e.g. the default Public channel) as chats even
  // before any message arrives - the store is otherwise only fed on message capture.
  the_mesh.uiSeedChannels();
  // Drop phantom channel chats left by the null/zero-key channel bug: a channel
  // convo whose slot is cleared (all-zero secret) isn't a real chat. Collect
  // first, then delete (deleteConvo reorders the list). Persists via loop's seq
  // save. The core fix (mm_channelEmpty) prevents new ones from being created.
  {
    mishmesh::ConvoKey drop[mishmesh::MAX_CONVOS];
    int nDrop = 0;
    for (int i = 0; i < _msgStore.convoCount() && nDrop < (int)mishmesh::MAX_CONVOS; i++) {
      mishmesh::ConvoSummary cs;
      if (!_msgStore.getConvo(i, cs) || cs.key.type != 1) continue;
      ChannelDetails cd;
      bool phantom = !the_mesh.getChannel(cs.key.id[0], cd);
      if (!phantom) {
        phantom = true;
        for (size_t b = 0; b < sizeof(cd.channel.secret); b++)
          if (cd.channel.secret[b]) { phantom = false; break; }
      }
      if (phantom) drop[nDrop++] = cs.key;
    }
    for (int i = 0; i < nDrop; i++) _msgStore.deleteConvo(drop[i]);
  }
  // [/mishmesh]

  // [mishmesh] bring up the sound subsystem and restore persisted prefs.
  _sound.begin(mishmesh::sound::defaultToneOutput());
  _sound.setVolume((mishmesh::sound::VolumeLevel)_node_prefs->sound_volume);
  _sound.setCategoryMask(_node_prefs->sound_mute_mask);
  // Master mute is intentionally NOT restored from the legacy buzzer_quiet pref:
  // mishmesh has no on-device mute toggle yet, so honoring a stale muted flag left by a
  // previously-flashed firmware would strand the device silent with no way to unmute.
  // Default audible; a future mishmesh sound-settings screen will own master mute.
  _sound.setMasterMute(false);
  mishmesh::sound::setActiveEngine(&_sound);
  // [/mishmesh]

  mishmesh::AppletContext ctx;
  ctx.app = this;
  ctx.contacts = this;
  // [mishmesh]
  ctx.messages = &_msgSvc;
  _theStorage.ds = the_mesh.getStore();
  ctx.storage = &_theStorage;
  mishmesh::quickReplyStore().begin(&_theStorage);   // load canned replies
  mishmesh::uiPrefs().begin(&_theStorage);   // battery style + home shortcuts
  mishmesh::clockService().begin(&_theStorage);   // alarm / world cities / timer duration
  ctx.sound = &_sound;          // [mishmesh]
  // [/mishmesh]
  _host = new mishmesh::AppletHost(_display, ctx);
  // [mishmesh]
  _host->setAutoOffMillis(mishmesh::screenSleepMillis(screenSleepIndex()));   // honor saved sleep pref
  // [/mishmesh]

  _menu = new mishmesh::AppMenuApplet();
  _lock = new mishmesh::LockApplet();
  _home = new mishmesh::HomeApplet();
  _home->setMenu(_menu);
  _home->setLock(_lock);
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
  _sound.play(mishmesh::sound::SoundId::BootJingle);   // [mishmesh]
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

void UITask::notify(UIEventType t) {
  // [mishmesh] Runs inside a mesh recv callback, BEFORE MyMesh appends the message
  // to the store. Delivery acks have no store dependency, so chirp now; message
  // notifications are deferred to loop() so the router sees the fresh store.
  using namespace mishmesh::sound;
  if (t == UIEventType::ack) {
    _sound.play(SoundId::MsgAck);
    if (_host) _host->requestRender();
    return;
  }
  if (t == UIEventType::contactMessage || t == UIEventType::channelMessage) {
    _notifyPending = true;
    _notifyEvent = t;
  }
  // [/mishmesh]
}

void UITask::dispatchNotification(UIEventType t) {
  // [mishmesh] The notification router. Picks the least-intrusive visual level for
  // an incoming message based on what the user is doing, and gates the alert sound
  // the same way (silent while the relevant chat is open). Called from loop(), so
  // the store already reflects the just-arrived message.
  using namespace mishmesh::sound;

  bool inbound = (t == UIEventType::contactMessage || t == UIEventType::channelMessage);
  if (!inbound) return;
  bool isChannel = (t == UIEventType::channelMessage);

  mishmesh::ConvoKey c;
  if (!_msgStore.lastInbound(c)) {                 // shouldn't happen post-capture
    _sound.play(mishmesh::sound::notifyTypeDefault(isChannel));
    if (_host) _host->requestRender();
    return;
  }

  mishmesh::ConvoKey active;
  bool inThisChat = _msgStore.activeConvo(active) && active.equals(c);
  bool sleeping   = _host && !_host->isDisplayOn();

  // Reading this very chat (awake): the thread surfaces the arrival itself.
  if (inThisChat && !sleeping) {
    if (_host) _host->requestRender();
    return;
  }

  // Per-chat notification level gate. DMs only ever store All or Mute, so the
  // MentionsOnly branch is effectively channel-only.
  mishmesh::NotifyLevel lvl = _msgSvc.notifyLevel(c);
  bool mention = false;
  if (c.type == 1 && lvl == mishmesh::NotifyLevel::MentionsOnly) {
    int mc = _msgStore.messageCount(c);
    mishmesh::MsgRecord mr;
    if (mc > 0 && _msgStore.getMessage(c, mc - 1, mr) && mr.text) {
      char body[mishmesh::MAX_TEXT + 1];
      uint16_t bl = mr.textLen < mishmesh::MAX_TEXT ? mr.textLen : mishmesh::MAX_TEXT;
      memcpy(body, mr.text, bl); body[bl] = 0;
      mention = mishmesh::textMentions(body, nodeName());
    }
  }
  bool alerts = (lvl == mishmesh::NotifyLevel::All) ||
                (lvl == mishmesh::NotifyLevel::MentionsOnly && mention);

  if (!alerts) {
    // Mute, or a non-mention in a mentions-only chat: list badge already updated
    // at capture; stay silent and out of the global count.
    if (_host) _host->requestRender();
    return;
  }

  _msgStore.markNotifiable(c);   // feeds totalNotifyUnread() (global indicator/bubble)
  {
    uint8_t typeByte = isChannel ? _node_prefs->notify_tone_ch : _node_prefs->notify_tone_dm;
    SoundId id;
    if (mishmesh::sound::resolveNotifyTone(_msgSvc.chatSound(c), typeByte,
                                           mishmesh::sound::notifyTypeDefault(isChannel), id))
      _sound.play(id);            // false -> Silent: skip the beep, visual path continues
  }
  if (!_host) return;

  bool atHome = _host->foreground() == _home;
  if (sleeping || atHome) {
    mishmesh::notificationApplet().raise(&_msgSvc, c);
    if (sleeping) _host->wakeDisplay();
    if (_host->foreground() != &mishmesh::notificationApplet())
      _host->push(&mishmesh::notificationApplet());
  } else {
    _host->postBubble(_msgSvc.totalNotifyUnread());
  }
  _host->requestRender();
  // [/mishmesh]
}

void UITask::loop() {
  // [mishmesh] debounced message-store persistence. Each change pushes the quiet
  // window out, but _msgDirtySince caps total deferral so a continuously-active
  // convo can't starve the write forever (which would lose mark-as-read across a
  // restart). Flush on whichever fires first.
  static const uint32_t MSG_FLUSH_DEBOUNCE_MS = 8000;   // quiet period before writing
  static const uint32_t MSG_FLUSH_MAX_DEFER_MS = 15000;  // hard cap on deferral under load
  if (_msgStore.seq() != _msgDirtySeq) {
    _msgDirtySeq = _msgStore.seq();
    _msgFlushAt  = millis() + MSG_FLUSH_DEBOUNCE_MS;
    if (_msgDirtySince == 0) _msgDirtySince = millis();   // start the cap on first dirty
  }
  bool capHit = _msgDirtySince && (millis() - _msgDirtySince >= MSG_FLUSH_MAX_DEFER_MS);
  if ((_msgFlushAt && millis() >= _msgFlushAt) || capHit) {
    _msgFlushAt = 0; _msgDirtySince = 0;
    _msgStore.saveIndex();   // [mishmesh] persist the in-RAM index to flash
  }
  // Drain a deferred message notification now that the store is up to date.
  if (_notifyPending) {
    _notifyPending = false;
    dispatchNotification(_notifyEvent);
  }

  // Auto-retry of undelivered direct messages: every ~2s, re-feed the still-
  // pending DMs to the engine and let it re-transmit / fail the due ones. The
  // store is the pending list, so this survives reboots for free.
  // Interval matches the retry cadence (RetryEngine::RETRY_INTERVAL_MS); scanning
  // faster only re-reads flash for no benefit. The scan walks every chat log, and
  // each backend read is a full file open on device, so it must stay cheap: one
  // pass collecting the pending DMs (the engine only tracks MAX_PENDING anyway),
  // not the O(n^2) count()+get() pattern. Deferred one interval so it never runs
  // during the boot jingle (a long blocking scan there would freeze the loop and
  // leave the tone stuck on).
  static const uint32_t RETRY_SCAN_MS = 6000;
  if (_retryScanAt == 0) _retryScanAt = millis() + RETRY_SCAN_MS;
  if (millis() >= _retryScanAt) {
    _retryScanAt = millis() + RETRY_SCAN_MS;
    mishmesh::MessagesConfig mc = _msgSvc.getMessagesConfig();
    _retry.configure(mc.autoRetry, mc.autoResetPath);
    if (mc.autoRetry) {
      uint32_t now = millis();
      _retry.beginScan();
      mishmesh::ConvoKey pk[mishmesh::RetryEngine::MAX_PENDING];
      uint32_t pt[mishmesh::RetryEngine::MAX_PENDING];
      int pn = _msgStore.collectPendingDMs(pk, pt, mishmesh::RetryEngine::MAX_PENDING);
      for (int i = 0; i < pn; i++) _retry.see(pk[i], pt[i], now);
      _retry.endScan(now, _retryGlue);
    } else {
      _retry.reset();
    }
  }
  // [/mishmesh]
  if (_host != nullptr) {
    // Clock engine: the stopwatch/timer/alarm keep counting while their UI is
    // closed; a fired timer/alarm raises the alert screen from any state.
    mishmesh::ClockEvent cev =
        mishmesh::clockService().tick(millis(), epochSeconds(), tzOffsetMinutes());
    if (cev != mishmesh::ClockEvent::None) {
      mishmesh::clockAlertApplet().raise(cev);
      if (!_host->isDisplayOn()) _host->wakeDisplay();
      if (_host->foreground() != &mishmesh::clockAlertApplet())
        _host->push(&mishmesh::clockAlertApplet());
      _host->requestRender();
    }
    // [mishmesh] one-shot "contacts full" banner. getNumContacts() is an O(1)
    // counter read, so polling every loop is cheap. Fires on the not-full -> full
    // transition when auto-add can no longer make room (overwrite off) and the
    // user hasn't disabled the alert; the latch re-arms when the store drops.
    {
      mishmesh::AutoAddConfig aac = getAutoAdd();
      uint16_t used = (uint16_t)the_mesh.getNumContacts();
      if (_contactsFullLatch.update(used, (uint16_t)MAX_CONTACTS,
                                    aac.notifyWhenFull, aac.overwriteOldest)) {
        mishmesh::contactsFullApplet().raise(used, (uint16_t)MAX_CONTACTS);
        if (!_host->isDisplayOn()) _host->wakeDisplay();
        if (_host->foreground() != &mishmesh::contactsFullApplet())
          _host->push(&mishmesh::contactsFullApplet());
        _host->requestRender();
      }
    }
    // [/mishmesh]
    _sound.tick(millis());   // [mishmesh]
    // [mishmesh] bank per-minute airtime deltas; cheap, advances buckets only on
    // minute rollover, and runs regardless of the active applet so the graph has
    // history even when it wasn't on screen.
    _airtime.tick(millis(), the_mesh.getTotalAirTime(), the_mesh.getReceiveAirTime());
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
  out.heardAt = c.lastmod;   // [mishmesh] our-clock receive time, for the Recent tab
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

// [mishmesh] Raw random access + change token backing the contacts list row cache.
int UITask::contactCount() const { return the_mesh.getNumContacts(); }

bool UITask::contactAt(int idx, mishmesh::ContactView& out) const {
  const ContactInfo* c = the_mesh.getContactPtrByIdx(idx);
  if (!c) return false;
  fillView(*c, out);   // ContactView points into the live record; no full-record copy
  return true;
}

// Cheap fingerprint of the contact table. Changes whenever a contact is added or
// removed, or a field that affects a rendered row / list ordering mutates (name,
// type, flags/favourite, path length) - regardless of which subsystem changed it,
// so the list cache invalidates without hooking every mutation site. One light
// pass reading a few bytes per contact; no ~190 B record copies.
uint32_t UITask::contactsSeq() const {
  uint32_t h = 2166136261u;   // FNV-1a
  int n = the_mesh.getNumContacts();
  h = (h ^ (uint32_t)n) * 16777619u;
  for (int i = 0; i < n; i++) {
    const ContactInfo* c = the_mesh.getContactPtrByIdx(i);
    if (!c) continue;
    for (const char* p = c->name; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    h = (h ^ c->type) * 16777619u;
    h = (h ^ c->flags) * 16777619u;                  // favourite bit lives here
    h = (h ^ c->out_path_len) * 16777619u;           // hop-count display
    h = (h ^ c->id.pub_key[0]) * 16777619u;          // catches reorder/replace
  }
  return h;
}

bool UITask::setFavourite(const uint8_t* pk, bool fav) { return the_mesh.uiSetFavourite(pk, fav); }
bool UITask::renameContact(const uint8_t* pk, const char* name) { return the_mesh.uiRenameContact(pk, name); }
// mishmesh TelemetryPerm bits == firmware TELEM_PERM_* values, so pass straight through.
uint8_t UITask::getTelemetryPerms(const uint8_t* pk) const { return the_mesh.uiGetTelemetryPerms(pk); }
bool UITask::setTelemetryPerm(const uint8_t* pk, uint8_t mask, bool on) { return the_mesh.uiSetTelemetryPerm(pk, mask, on); }
bool UITask::getPath(const uint8_t* pk, uint8_t* out, uint8_t& len) const { return the_mesh.uiGetPath(pk, out, len); }
bool UITask::setPath(const uint8_t* pk, const uint8_t* path, uint8_t len) { return the_mesh.uiSetPath(pk, path, len); }

int UITask::countDiscovered() const { return the_mesh.uiDiscoveryCount(); }
bool UITask::getDiscovered(int index, mishmesh::ContactView& out) const {
  if (!the_mesh.uiGetDiscovery(index, _scratch)) return false;
  fillView(_scratch, out);
  return true;
}
bool UITask::addDiscovered(const uint8_t* pk) { return the_mesh.uiAddDiscovery(pk); }

// [mishmesh]
int UITask::countRecentAdverts() const { return the_mesh.uiRecentAdvertCount(); }
bool UITask::getRecentAdvert(int index, mishmesh::ContactView& out) const {
  if (!the_mesh.uiGetRecentAdvert(index, _scratch)) return false;
  fillView(_scratch, out);
  return true;
}
bool UITask::isContact(const uint8_t* pk) const { return the_mesh.lookupContactByPubKey(pk, 6) != nullptr; }
// [/mishmesh]

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

// [mishmesh] Room-server login. The result arrives asynchronously via
// onRoomLogin() (from MyMesh::onContactResponse), which bumps _loginSeq.
bool UITask::login(const uint8_t* pk, const char* password) {
  return the_mesh.mishmeshLogin(pk, password);
}

void UITask::onRoomLogin(const uint8_t* pubkey, bool success, bool is_admin, uint8_t perms) {
  memcpy(_loginPub, pubkey, 6);
  _loginOk = success;
  _loginAdmin = is_admin;
  _loginPerms = perms;
  _loginSeq++;
  if (success) {
    // Remember for this power cycle so re-opening the room skips the prompt.
    for (int i = 0; i < _loggedInCount; i++)
      if (memcmp(_loggedIn[i], pubkey, 6) == 0) return;
    if (_loggedInCount < MM_MAX_LOGINS) {
      memcpy(_loggedIn[_loggedInCount++], pubkey, 6);
    } else {
      memcpy(_loggedIn[0], pubkey, 6);   // ring-evict oldest
      for (int i = 0; i + 1 < MM_MAX_LOGINS; i++) memcpy(_loggedIn[i], _loggedIn[i + 1], 6);
      memcpy(_loggedIn[MM_MAX_LOGINS - 1], pubkey, 6);
    }
  }
}

bool UITask::loginResult(const uint8_t* pk, bool& ok, bool& isAdmin, uint8_t& perms) const {
  if (memcmp(_loginPub, pk, 6) != 0) return false;
  ok = _loginOk; isAdmin = _loginAdmin; perms = _loginPerms;
  return true;
}

bool UITask::isLoggedIn(const uint8_t* pk) const {
  for (int i = 0; i < _loggedInCount; i++)
    if (memcmp(_loggedIn[i], pk, 6) == 0) return true;
  return false;
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
  c.notifyWhenFull = p->contacts_full_notify != 0;
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
  p->contacts_full_notify = c.notifyWhenFull ? 1 : 0;
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
    // Empty name (cleared/unnamed slot) falls through to "#idx", never blank.
    if (the_mesh.getChannel(k.id[0], cd) && cd.name[0] != '\0') {
      snprintf(buf, sizeof(buf), "%s", cd.name); return buf;
    }
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
  // preview: the store keeps a truncated last-message preview per convo in RAM
  // (ConvoSummary.preview) precisely so the list never reads flash per row.
  // Reading the last message via getMessage() here streams the whole chat log
  // from flash for every visible row on every navigation - pathologically slow.
  // Copy into a static buffer so the returned pointer outlives the local `c`.
  static char prevBuf[48];
  size_t n = 0;
  while (c.preview[n] && n < sizeof(prevBuf) - 1) { prevBuf[n] = c.preview[n]; n++; }
  prevBuf[n] = '\0';
  out.preview = prevBuf;
  return true;
}

uint16_t UITask::MsgSvc::totalUnread() const { return store->totalUnread(); }
uint16_t UITask::MsgSvc::totalNotifyUnread() const { return store->totalNotifyUnread(); }
int  UITask::MsgSvc::messageCount(const mishmesh::ConvoKey& k) const { return store->messageCount(k); }

bool UITask::MsgSvc::getMessage(const mishmesh::ConvoKey& k, int i, mishmesh::MessageView& out) const {
  mishmesh::MsgRecord r;
  if (!store->getMessage(k, i, r)) return false;
  buildView(r, k, out);
  return true;
}

// Single sequential flash pass over a chat's log, converting each record to a
// MessageView, O(n) vs n separate getMessage() calls, used for full-thread
// relayout on chat open. The view (and its static text buffer) is valid only
// during the visitor call.
void UITask::MsgSvc::forEachMessage(const mishmesh::ConvoKey& k, MsgVisitor visit, void* ctx) const {
  struct Ctx { const MsgSvc* self; mishmesh::ConvoKey k; MsgVisitor v; void* c; } cx{ this, k, visit, ctx };
  store->forEachMessage(k, [](void* p, int idx, const mishmesh::MsgRecord& r) {
    auto* cx = static_cast<Ctx*>(p);
    mishmesh::MessageView mv;
    cx->self->buildView(r, cx->k, mv);
    cx->v(cx->c, idx, mv);
  }, &cx);
}

void UITask::MsgSvc::buildView(const mishmesh::MsgRecord& r, const mishmesh::ConvoKey& k,
                               mishmesh::MessageView& out) const {
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
  // Surface the live auto-retry attempt for a still-pending DM (drives the
  // "Retrying N/5" label in the thread view).
  out.retryAttempt = 0;
  if (retry && out.outbound && !out.isChannel && r.status == mishmesh::ST_PENDING) {
    uint8_t a = 0;
    if (retry->attemptsFor(k, r.senderTime, a)) out.retryAttempt = a;
  }

  // copy text to null-terminated buffer; for inbound channel split "name: body"
  static char textBuf[mishmesh::MAX_TEXT + 1];
  static char senderBuf[34];
  uint16_t n = r.textLen < mishmesh::MAX_TEXT ? r.textLen : (uint16_t)mishmesh::MAX_TEXT;
  memcpy(textBuf, r.text, n);
  textBuf[n] = '\0';
  out.senderName = "";
  out.text       = textBuf;
  // Channels and room-server posts both carry the sender inline as "Name: body"
  // (rooms are stored that way in MyMesh::queueMessage). Split so the thread can
  // caption each inbound post. isChannel stays false for rooms so their outbound
  // posts keep DM-style ACK/retry status.
  bool splitSender = r.kind == mishmesh::KIND_INBOUND &&
                     (k.type == 1 || the_mesh.mishmeshIsRoomConvo(k));
  if (splitSender) {
    char* sep = strstr(textBuf, ": ");
    if (sep) {
      *sep = '\0';
      snprintf(senderBuf, sizeof(senderBuf), "%s", textBuf);
      out.senderName = senderBuf;
      out.text       = sep + 2;
    }
  }
}

void UITask::MsgSvc::setActiveConvo(const mishmesh::ConvoKey& k) { store->setActiveConvo(k); }
void UITask::MsgSvc::clearActiveConvo() { store->clearActiveConvo(); }
int  UITask::MsgSvc::repeatCount(const mishmesh::ConvoKey& k, int m) const { return store->repeatCount(k, m); }

bool UITask::MsgSvc::getRepeat(const mishmesh::ConvoKey& k, int m, int r, mishmesh::RepeatView& out) const {
  mishmesh::RepeatRec rr;
  if (!store->getRepeat(k, m, r, rr)) return false;
  out.hops    = rr.hops;
  out.snrx4   = rr.snrx4;
  out.path    = rr.path;
  out.pathLen = rr.pathLen;
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
  // Fall back to the Discover feed (adverts heard but not added as contacts) so a
  // repeater we've seen advertise still resolves to a name instead of a bare hex
  // hash. Contacts win; discoveries don't bump knownCount (it counts contacts).
  if (!hasFirst) {
    int d = the_mesh.uiDiscoveryCount();
    for (int i = 0; i < d; i++) {
      if (!the_mesh.uiGetDiscovery(i, ci)) continue;
      if (ci.id.pub_key[0] == hashByte) {
        snprintf(buf, sizeof(buf), "%s", ci.name); hasFirst = true; break;
      }
    }
  }
  name = hasFirst ? buf : "";
  return hasFirst;
}

void UITask::MsgSvc::deleteMessage(const mishmesh::ConvoKey& k, int i) { store->deleteMessage(k, i); }
void UITask::MsgSvc::clearConvo(const mishmesh::ConvoKey& k) { store->clearConvo(k); }
void UITask::MsgSvc::deleteConvo(const mishmesh::ConvoKey& k) {
#ifdef MAX_GROUP_CHANNELS
  if (k.type == 1) {                       // leaving a channel: free the mesh slot, not just the chat
    ChannelDetails empty;
    memset(&empty, 0, sizeof(empty));      // zero name + secret = the protocol's "delete channel" form
    if (the_mesh.setChannel(k.id[0], empty)) the_mesh.getStore()->saveChannels(&the_mesh);
  }
#endif
  store->deleteConvo(k);
}
void UITask::MsgSvc::markUnread(const mishmesh::ConvoKey& k) { store->markUnread(k); }
uint32_t UITask::MsgSvc::seq() const { return store->seq(); }

// Per-chat region storage key: "rgc<idx>" for channels, "rg_<12 hex>" for DMs
// (the 6-byte pubkey prefix). Empty/absent value means "None" (node default).
static void regionStorageKey(const mishmesh::ConvoKey& k, char* buf, size_t n) {
  if (k.type == 1) snprintf(buf, n, "rgc%u", (unsigned)k.id[0]);
  else snprintf(buf, n, "rg_%02x%02x%02x%02x%02x%02x",
                k.id[0], k.id[1], k.id[2], k.id[3], k.id[4], k.id[5]);
}

int UITask::MsgSvc::region(const mishmesh::ConvoKey& k, char* dst, int cap) const {
  if (dst && cap > 0) dst[0] = 0;
  if (!storage || !dst || cap <= 1) return 0;
  char key[20];
  regionStorageKey(k, key, sizeof(key));
  uint8_t want = (uint8_t)((cap - 1) < 30 ? (cap - 1) : 30);
  uint8_t got = storage->load(key, (uint8_t*)dst, want);
  dst[got] = 0;
  return (int)strlen(dst);   // a 1-byte 0 "cleared" marker reads back as length 0
}

void UITask::MsgSvc::setRegion(const mishmesh::ConvoKey& k, const char* name) {
  if (!storage) return;
  char key[20];
  regionStorageKey(k, key, sizeof(key));
  char tmp[31];
  int len = 0;
  if (name) for (; name[len] && len < 30; len++) tmp[len] = name[len];
  if (len == 0) { uint8_t zero = 0; storage->save(key, &zero, 1); return; }   // cleared marker
  storage->save(key, (const uint8_t*)tmp, (uint8_t)len);
}

// Per-chat notification-level storage key: "nfc<idx>" for channels,
// "nf_<12 hex>" for DMs. Absent value means All (the default).
static void notifyStorageKey(const mishmesh::ConvoKey& k, char* buf, size_t n) {
  if (k.type == 1) snprintf(buf, n, "nfc%u", (unsigned)k.id[0]);
  else snprintf(buf, n, "nf_%02x%02x%02x%02x%02x%02x",
                k.id[0], k.id[1], k.id[2], k.id[3], k.id[4], k.id[5]);
}

mishmesh::NotifyLevel UITask::MsgSvc::notifyLevel(const mishmesh::ConvoKey& k) const {
  if (!storage) return mishmesh::NotifyLevel::All;
  char key[20]; notifyStorageKey(k, key, sizeof(key));
  uint8_t b = 0;
  uint8_t got = storage->load(key, &b, 1);
  if (got == 0 || b > (uint8_t)mishmesh::NotifyLevel::Mute) return mishmesh::NotifyLevel::All;
  return (mishmesh::NotifyLevel)b;
}

void UITask::MsgSvc::setNotifyLevel(const mishmesh::ConvoKey& k, mishmesh::NotifyLevel lvl) {
  if (!storage) return;
  char key[20]; notifyStorageKey(k, key, sizeof(key));
  uint8_t b = (uint8_t)lvl;
  storage->save(key, &b, 1);   // All writes a 0 byte that loads back as the default
}

// Per-chat ringtone storage key: "sfc<idx>" for channels, "sf_<12 hex>" for DMs.
// Absent value means Default (0) -> fall through to the per-type default.
static void soundStorageKey(const mishmesh::ConvoKey& k, char* buf, size_t n) {
  if (k.type == 1) snprintf(buf, n, "sfc%u", (unsigned)k.id[0]);
  else snprintf(buf, n, "sf_%02x%02x%02x%02x%02x%02x",
                k.id[0], k.id[1], k.id[2], k.id[3], k.id[4], k.id[5]);
}

uint8_t UITask::MsgSvc::chatSound(const mishmesh::ConvoKey& k) const {
  if (!storage) return 0;
  char key[20]; soundStorageKey(k, key, sizeof(key));
  uint8_t b = 0;
  uint8_t got = storage->load(key, &b, 1);
  return got == 0 ? 0 : b;
}

void UITask::MsgSvc::setChatSound(const mishmesh::ConvoKey& k, uint8_t encoded) {
  if (!storage) return;
  char key[20]; soundStorageKey(k, key, sizeof(key));
  storage->save(key, &encoded, 1);   // 0 (Default) writes a 0 byte that reads back as Default
}

// Global Messages settings. autoRetry/autoResetPath have no firmware mechanism
// yet, so they live only as a UI bitmask in AppletStorage ("msgcfg"). directAcks
// maps onto NodePrefs.multi_acks (0 -> 1 ack, 1 -> 2 acks).
#define MSGCFG_AUTO_RETRY      0x01
#define MSGCFG_AUTO_RESET_PATH 0x02

mishmesh::MessagesConfig UITask::MsgSvc::getMessagesConfig() const {
  if (!_msgFlagsLoaded) {                 // load the file once, then serve from RAM
    _msgFlags = 0;
    if (storage) storage->load("msgcfg", &_msgFlags, 1);
    _msgFlagsLoaded = true;
  }
  mishmesh::MessagesConfig c;
  c.autoRetry     = (_msgFlags & MSGCFG_AUTO_RETRY) != 0;
  c.autoResetPath = (_msgFlags & MSGCFG_AUTO_RESET_PATH) != 0;
  NodePrefs* p = the_mesh.getNodePrefs();
  c.directAcks = (p && p->multi_acks >= 1) ? 2 : 1;
  return c;
}

void UITask::MsgSvc::setMessagesConfig(const mishmesh::MessagesConfig& c) {
  uint8_t flags = 0;
  if (c.autoRetry)     flags |= MSGCFG_AUTO_RETRY;
  if (c.autoResetPath) flags |= MSGCFG_AUTO_RESET_PATH;
  if (storage) storage->save("msgcfg", &flags, 1);
  _msgFlags = flags; _msgFlagsLoaded = true;   // keep the cache hot
  NodePrefs* p = the_mesh.getNodePrefs();
  if (p) {
    p->multi_acks = (c.directAcks >= 2) ? 1 : 0;
    the_mesh.savePrefs();
  }
}

// --- auto-retry glue: the engine's decisions executed against the radio/store ---
void UITask::RetryGlue::retransmit(const mishmesh::ConvoKey& k, uint32_t senderTime,
                                   uint8_t attempt, bool resetPath) {
  the_mesh.mishmeshRetransmit(k, senderTime, attempt, resetPath);
}
void UITask::RetryGlue::markFailed(const mishmesh::ConvoKey& k, uint32_t senderTime) {
  if (store) store->markFailed(k, senderTime);
}

// Derives the 16-byte flood-scope key for a chat's region, matching the firmware
// convention (sha256 of "#"+name, truncated to 16B - same as default scope and
// hashtag channels). Returns false when the chat has no region (use node default).
static bool resolveRegionScope(const mishmesh::MessagesService& svc,
                               const mishmesh::ConvoKey& k, uint8_t out16[16]) {
  char name[31];
  if (svc.region(k, name, sizeof(name)) <= 0) return false;
  char named[33];
  snprintf(named, sizeof(named), "#%s", name);
  mesh::Utils::sha256(out16, 16, (const uint8_t*)named, (int)strlen(named));
  return true;
}

bool UITask::MsgSvc::sendText(const mishmesh::ConvoKey& k, const char* text) {
  uint8_t scope[16];
  bool scoped = resolveRegionScope(*this, k, scope);
  return the_mesh.mishmeshSendText(k, text, scoped ? scope : nullptr);
}

const uint8_t* UITask::MsgSvc::publicPsk() {
  // 8b3387e9c5cdea6ac9e5edbaa115cd72 - the well-known public-channel key.
  static const uint8_t k[16] = {
    0x8b,0x33,0x87,0xe9,0xc5,0xcd,0xea,0x6a,
    0xc9,0xe5,0xed,0xba,0xa1,0x15,0xcd,0x72,
  };
  return k;
}

int UITask::MsgSvc::freeChannelSlot() const {
#ifdef MAX_GROUP_CHANNELS
  ChannelDetails ch;
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    if (!the_mesh.getChannel(i, ch)) continue;
    bool empty = true;                          // unused slot = all-zero secret
    for (int b = 0; b < PUB_KEY_SIZE; b++) if (ch.channel.secret[b]) { empty = false; break; }
    if (empty) return i;
  }
#endif
  return -1;
}

mishmesh::ChanResult UITask::MsgSvc::setSecret(const char* name, const uint8_t secret16[16]) {
#ifdef MAX_GROUP_CHANNELS
  // Already joined this exact key? Reuse its slot instead of allocating a second
  // one - a duplicate slot would split send (newest slot) from receive
  // (findChannelIdx picks the lowest-index match).
  mesh::GroupChannel probe;
  memset(&probe, 0, sizeof(probe));
  memcpy(probe.secret, secret16, 16);
  int existing = the_mesh.findChannelIdx(probe);
  if (existing >= 0) {
    if (store) store->ensureChannel((uint8_t)existing);   // surface the chat
    return mishmesh::ChanResult::Duplicate;
  }
#endif
  int idx = freeChannelSlot();
  if (idx < 0) return mishmesh::ChanResult::Full;
  ChannelDetails ch;
  memset(&ch, 0, sizeof(ch));
  StrHelper::strncpy(ch.name, name, sizeof(ch.name));
  memcpy(ch.channel.secret, secret16, 16);      // 128-bit; setChannel computes the hash
  if (!the_mesh.setChannel(idx, ch)) return mishmesh::ChanResult::Error;
  the_mesh.getStore()->saveChannels(&the_mesh);
  if (store) store->ensureChannel((uint8_t)idx); // seed the chat so it shows in Chats
  return mishmesh::ChanResult::Ok;
}

mishmesh::ChanResult UITask::MsgSvc::createPrivateChannel(const char* name) {
  if (!name || !name[0]) return mishmesh::ChanResult::Invalid;
  uint8_t secret[16];
  the_mesh.getRNG()->random(secret, sizeof(secret));
  return setSecret(name, secret);
}

mishmesh::ChanResult UITask::MsgSvc::joinPrivateChannel(const char* name, const char* keyHex) {
  if (!name || !name[0]) return mishmesh::ChanResult::Invalid;
  // Accept exactly 32 hex chars (stripping nothing; the applet validates charset).
  if (!keyHex) return mishmesh::ChanResult::Invalid;
  size_t n = strlen(keyHex);
  if (n != 32) return mishmesh::ChanResult::Invalid;
  uint8_t secret[16];
  if (!mesh::Utils::fromHex(secret, sizeof(secret), keyHex)) return mishmesh::ChanResult::Invalid;
  return setSecret(name, secret);
}

mishmesh::ChanResult UITask::MsgSvc::joinPublicChannel() {
  if (publicChannelJoined()) return mishmesh::ChanResult::Duplicate;
  return setSecret("Public", publicPsk());
}

mishmesh::ChanResult UITask::MsgSvc::joinHashtagChannel(const char* hashtag) {
  if (!hashtag || !hashtag[0]) return mishmesh::ChanResult::Invalid;
  const char* base = (hashtag[0] == '#') ? hashtag + 1 : hashtag;   // strip one leading #
  if (!base[0]) return mishmesh::ChanResult::Invalid;
  char named[33];                                                    // "#" + up to 31-char base + NUL
  snprintf(named, sizeof(named), "#%s", base);                       // display + hash input
  uint8_t secret[16];
  mesh::Utils::sha256(secret, sizeof(secret),
                      (const uint8_t*)named, (int)strlen(named));     // first 16 bytes
  return setSecret(named, secret);
}

bool UITask::MsgSvc::publicChannelJoined() const {
#ifdef MAX_GROUP_CHANNELS
  ChannelDetails ch;
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    if (!the_mesh.getChannel(i, ch)) continue;
    if (memcmp(ch.channel.secret, publicPsk(), 16) == 0) return true;
  }
#endif
  return false;
}

bool UITask::MsgSvc::channelKeyHex(const mishmesh::ConvoKey& k, char* dst, int cap) const {
  if (dst && cap > 0) dst[0] = 0;
  if (k.type != 1 || !dst || cap < 33) return false;   // channels only; 32 hex + NUL
  ChannelDetails cd;
  if (!the_mesh.getChannel(k.id[0], cd)) return false;
  mesh::Utils::toHex(dst, cd.channel.secret, 16);       // first 16 bytes = the PSK
  return true;
}

// --- MishmeshStorage: filesystem key->blob persistence for applets ---
// Files live under /mm/<key> on the secondary FS (fallback primary), capped at 128 B.

static void mmPath(char* buf, size_t size, const char* key) {
  snprintf(buf, size, "/mm/%s", key);
}

uint8_t UITask::MishmeshStorage::load(const char* key, uint8_t* dst, uint8_t cap) {
  if (!ds || !key || !dst || cap == 0) return 0;
  FILESYSTEM* fs = ds->getSecondaryFS() ? ds->getSecondaryFS() : ds->getPrimaryFS();
  if (!fs) return 0;
  char path[40];
  mmPath(path, sizeof(path), key);
  File f = ds->openRead(fs, path);
  if (!f) return 0;
  uint8_t toRead = cap < 128 ? cap : 128;
  uint8_t n = (uint8_t)f.read(dst, toRead);
  f.close();
  return n;
}

bool UITask::MishmeshStorage::save(const char* key, const uint8_t* src, uint8_t len) {
  if (!ds || !key || !src || len == 0) return false;
  FILESYSTEM* fs = ds->getSecondaryFS() ? ds->getSecondaryFS() : ds->getPrimaryFS();
  if (!fs) return false;
  fs->mkdir("/mm");
  char path[40];
  mmPath(path, sizeof(path), key);
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(path);
  File f = fs->open(path, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  File f = fs->open(path, "w");
#else
  File f = fs->open(path, "w", true);
#endif
  if (!f) return false;
  uint8_t toWrite = len < 128 ? len : 128;
  size_t w = f.write(src, toWrite);
  f.close();
  return w == (size_t)toWrite;
}

// [/mishmesh]
