#pragma once

// [mishmesh] lets main.cpp swap the generic "Loading..." boot text for our splash
#define MISHMESH_UI 1

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <Arduino.h>

#include "../AbstractUITask.h"
#include "../NodePrefs.h"
#include "../MyMesh.h"            // the_mesh, MyMesh::ui* hooks, ContactInfo

#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/InputSources.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/applets/HomeApplet.h>
#include <mishmesh/applets/AppMenuApplet.h>
#include <mishmesh/applets/LockApplet.h>
// [mishmesh]
#include <mishmesh/core/WorldClock.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/RetryEngine.h>
#include <mishmesh/core/ScreenSleep.h>
#include <mishmesh/core/NameValidation.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/core/AirtimeHistory.h>
#include <mishmesh/core/ContactsFullLatch.h>
#include <mishmesh/applets/ContactsFullApplet.h>
#include <mishmesh/core/ExtraFsMsgBackend.h>
#include <mishmesh/applets/OnboardingApplet.h>
// [/mishmesh]

class UITask : public AbstractUITask, public mishmesh::AppServices, public mishmesh::ContactsService {
  DisplayDriver* _display;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  mishmesh::AppletHost* _host;
  mishmesh::HomeApplet* _home;
  mishmesh::AppMenuApplet* _menu;
  mishmesh::LockApplet* _lock;
  mishmesh::OnboardingApplet* _onboard = nullptr;   // [mishmesh] first-boot wizard (null after)

  mutable uint16_t _batt_mv;        // smoothed; raw ADC reads are noisy
  mutable uint32_t _batt_sampled_at;
  mutable uint32_t _heap_min = 0;   // free-heap low watermark; nRF52 has no built-in one

  mutable ContactInfo _scratch;     // backs the ContactView returned by getByKind

  static void fillView(const ContactInfo& c, mishmesh::ContactView& out);

  // [mishmesh]
  mishmesh::ExtraFsMsgBackend _backend;  // flash backend; wired to store in begin()
  mishmesh::MessageStore _msgStore;
  uint32_t _msgDirtySeq = 0;
  uint32_t _msgFlushAt  = 0;
  uint32_t _msgDirtySince = 0;   // when the store first went dirty; caps flush deferral

  struct MsgSvc : public mishmesh::MessagesService {
    mishmesh::MessageStore* store = nullptr;
    mishmesh::AppletStorage* storage = nullptr;   // per-chat region persistence
    mishmesh::RetryEngine* retry = nullptr;       // live retry-attempt count for pending DMs
    // Cached "msgcfg" flags byte: getMessagesConfig() runs per settings row per
    // render frame, so it must not hit the filesystem each call. Loaded lazily,
    // written through on set.
    mutable uint8_t _msgFlags = 0;
    mutable bool    _msgFlagsLoaded = false;

    const char* nameFor(const mishmesh::ConvoKey& k) const;
    int  convoCount() const override;
    bool getConvo(int i, mishmesh::ConvoView& out) const override;
    uint16_t totalUnread() const override;
    uint16_t totalNotifyUnread() const override;
    int  messageCount(const mishmesh::ConvoKey& k) const override;
    bool getMessage(const mishmesh::ConvoKey& k, int i, mishmesh::MessageView& out) const override;
    void forEachMessage(const mishmesh::ConvoKey& k, MsgVisitor visit, void* ctx) const override;
    void buildView(const mishmesh::MsgRecord& r, const mishmesh::ConvoKey& k,
                   mishmesh::MessageView& out) const;   // MsgRecord -> MessageView (shared)
    void setActiveConvo(const mishmesh::ConvoKey& k) override;
    void clearActiveConvo() override;
    int  repeatCount(const mishmesh::ConvoKey& k, int m) const override;
    bool getRepeat(const mishmesh::ConvoKey& k, int m, int r, mishmesh::RepeatView& out) const override;
    bool resolveHop(uint8_t hashByte, const char*& name, uint8_t& knownCount) const override;
    void deleteMessage(const mishmesh::ConvoKey& k, int i) override;
    void clearConvo(const mishmesh::ConvoKey& k) override;
    void deleteConvo(const mishmesh::ConvoKey& k) override;
    void markUnread(const mishmesh::ConvoKey& k) override;
    uint32_t seq() const override;
    bool sendText(const mishmesh::ConvoKey& k, const char* text) override;
    int  region(const mishmesh::ConvoKey& k, char* dst, int cap) const override;
    void setRegion(const mishmesh::ConvoKey& k, const char* name) override;
    mishmesh::NotifyLevel notifyLevel(const mishmesh::ConvoKey& k) const override;
    void setNotifyLevel(const mishmesh::ConvoKey& k, mishmesh::NotifyLevel lvl) override;
    uint8_t chatSound(const mishmesh::ConvoKey& k) const override;
    void setChatSound(const mishmesh::ConvoKey& k, uint8_t encoded) override;
    mishmesh::MessagesConfig getMessagesConfig() const override;
    void setMessagesConfig(const mishmesh::MessagesConfig& cfg) override;
    mishmesh::ChanResult createPrivateChannel(const char* name) override;
    mishmesh::ChanResult joinPrivateChannel(const char* name, const char* keyHex) override;
    mishmesh::ChanResult joinPublicChannel() override;
    mishmesh::ChanResult joinHashtagChannel(const char* hashtag) override;
    bool publicChannelJoined() const override;
    bool channelKeyHex(const mishmesh::ConvoKey& k, char* dst, int cap) const override;
  private:
    int freeChannelSlot() const;                                   // -1 if full
    mishmesh::ChanResult setSecret(const char* name, const uint8_t secret16[16]);
    static const uint8_t* publicPsk();                             // 16 bytes
  } _msgSvc;

  // Filesystem-backed AppletStorage: one small file per key under /mm/ on the
  // secondary FS (fallback primary). Capped at 128 bytes per key.
  struct MishmeshStorage : public mishmesh::AppletStorage {
    DataStore* ds = nullptr;
    uint8_t load(const char* key, uint8_t* dst, uint8_t cap) override;
    bool    save(const char* key, const uint8_t* src, uint8_t len) override;
  } _theStorage;

  // Auto-retry of undelivered direct messages. The engine decides what/when;
  // this glue performs the radio re-transmit (via MyMesh) and the failed-flag.
  struct RetryGlue : public mishmesh::RetryActions {
    mishmesh::MessageStore* store = nullptr;
    void retransmit(const mishmesh::ConvoKey& k, uint32_t senderTime,
                    uint8_t attempt, bool resetPath) override;
    void markFailed(const mishmesh::ConvoKey& k, uint32_t senderTime) override;
  } _retryGlue;
  mishmesh::RetryEngine _retry;
  uint32_t _retryScanAt = 0;   // next due time for the pending-message scan

  mishmesh::sound::SoundEngine _sound;

  // Per-minute airtime history for the Airtime applet. Sampled in loop() from the
  // Dispatcher's cumulative counters so it accrues even while the screen is off.
  mishmesh::AirtimeHistory _airtime;

  // One-shot "contacts full" latch; fed live counts in loop(). Fires the banner on
  // the not-full -> full transition when overwrite is off and the alert is enabled.
  mishmesh::ContactsFullLatch _contactsFullLatch;

  // Notification routing is deferred from notify() to the next loop(): notify()
  // fires from a mesh recv callback that runs BEFORE MyMesh appends the message to
  // the store, so the store (unread totals, latest message) is only fresh one step
  // later. loop() drains this and dispatches against the up-to-date store.
  bool        _notifyPending = false;
  UIEventType _notifyEvent = UIEventType::none;
  void dispatchNotification(UIEventType t);
  void applyTimeSyncGate(bool on);
  // [mishmesh] per-minute memo for the DST-aware tz resolver (see tzOffsetMinutes)
  mutable uint32_t _tzCacheMin = 0xFFFFFFFFu;
  mutable int16_t  _tzCacheOff = 0;
  // [/mishmesh]

#ifdef UI_HAS_JOYSTICK
  mishmesh::DirectionalSource* _joystick;
  mishmesh::ButtonGestureSource* _backBtn;
#else
  mishmesh::ButtonGestureSource* _userBtn;
#endif

public:
  UITask(mesh::MainBoard* board, BaseSerialInterface* serial)
      : AbstractUITask(board, serial),
        _display(nullptr), _sensors(nullptr), _node_prefs(nullptr),
        _host(nullptr), _home(nullptr), _menu(nullptr), _lock(nullptr),
        _batt_mv(0), _batt_sampled_at(0) {
#ifdef UI_HAS_JOYSTICK
    _joystick = nullptr;
    _backBtn = nullptr;
#else
    _userBtn = nullptr;
#endif
  }

  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);
  void finishOnboardingToHome();   // [mishmesh] called by onboardingDone callback

  // [mishmesh] one-shot boot splash drawn from setup() before any applet exists;
  // touches only the passed display, so it runs before begin().
  static void drawBootSplash(DisplayDriver* disp);

  // mishmesh::AppServices
  const char* nodeName() const override { return _node_prefs ? _node_prefs->node_name : ""; }
  uint16_t batteryMillivolts() const override;
  uint32_t epochSeconds() const override;
  bool systemStats(mishmesh::SystemStats& out) const override;
  bool airtimeStats(mishmesh::AirtimeStats& out) const override;
  // [mishmesh]
  void factoryReset(bool keepIdentity) override { the_mesh.uiFactoryReset(keepIdentity); }
  // [/mishmesh]
  // [mishmesh]
  void selfPublicKeyHex(char* out, size_t cap, int bytes) const override {
    if (!out || !cap) return;
    if (bytes < 1) bytes = 1;
    if ((size_t)(2 * bytes + 1) > cap) bytes = (int)((cap - 1) / 2);
    mesh::Utils::toHex(out, the_mesh.self_id.pub_key, bytes);
  }
  void markOnboardingComplete() override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (p) { p->onboarding_state = 2; the_mesh.savePrefs(); }
  }
  void resetOnboarding() override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (p) { p->onboarding_state = 1; the_mesh.savePrefs(); }   // IN_PROGRESS: gate re-shows the wizard
    _board->reboot();   // does not return
  }
  // [/mishmesh]
  // [mishmesh]
  // BLE capability is a build-time fact: deriving it from runtime state
  // (pin/enable) made the Bluetooth settings entry vanish when BLE was
  // toggled off. Serial/USB builds have no BLE at all, so tile/entry hide.
  bool bleSupported() const override {
#ifdef BLE_PIN_CODE
    return true;
#else
    return false;
#endif
  }
  bool bleEnabled()   const override { return isSerialEnabled(); }
  // Gate on enabled: the USB serial's isConnected() is a hardwired `true`,
  // and even on BLE a stale connection flag must not outlive a disable.
  bool bleConnected() const override { return isSerialEnabled() && hasConnection(); }
  uint32_t blePin()   const override { return the_mesh.getBLEPin(); }
  void setBleEnabled(bool on) override {
    if (on) enableSerial(); else disableSerial();
    // Persist so the choice survives reboot; startInterface() re-enables the
    // link on every boot, so UITask::begin() re-applies this on startup.
    if (_node_prefs) { _node_prefs->ble_enabled = on ? 1 : 0; the_mesh.savePrefs(); }
  }
  bool sendAdvert(bool flood) override { return the_mesh.sendSelfAdvert(flood); }
  bool shareLocationInAdvert() const override {
    NodePrefs* p = the_mesh.getNodePrefs();
    return p && p->advert_loc_policy == ADVERT_LOC_SHARE;
  }
  void setShareLocationInAdvert(bool on) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    p->advert_loc_policy = on ? ADVERT_LOC_SHARE : ADVERT_LOC_NONE;
    the_mesh.savePrefs();
  }
  void setSoundVolume(uint8_t level) override {
    _sound.setVolume((mishmesh::sound::VolumeLevel)level);
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    p->sound_volume = level;
    the_mesh.savePrefs();
  }
  uint8_t notifyTone(bool channel) const override {
    return _node_prefs ? (channel ? _node_prefs->notify_tone_ch : _node_prefs->notify_tone_dm) : 0;
  }
  void setNotifyTone(bool channel, uint8_t encoded) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    if (channel) p->notify_tone_ch = encoded; else p->notify_tone_dm = encoded;
    the_mesh.savePrefs();
  }
  bool radioConfig(mishmesh::RadioConfig& out) const override {
    if (!_node_prefs) return false;
    out.freqMhz    = _node_prefs->freq;
    out.bwKhz      = _node_prefs->bw;
    out.sf         = _node_prefs->sf;
    out.cr         = _node_prefs->cr;
    out.txPowerDbm = _node_prefs->tx_power_dbm;
    out.repeater   = _node_prefs->client_repeat != 0;
    return true;
  }
  int8_t txPowerMax() const override { return the_mesh.uiTxPowerMax(); }
  void setRadioConfig(const mishmesh::RadioConfig& c) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    p->freq = c.freqMhz; p->bw = c.bwKhz; p->sf = c.sf; p->cr = c.cr;
    p->tx_power_dbm = c.txPowerDbm;
    // Off-grid repeat only where the frequency is a permitted repeat band.
    bool allowRepeat = c.repeater &&
        the_mesh.uiIsValidRepeatFreq((uint32_t)(c.freqMhz * 1000.0f + 0.5f));
    p->client_repeat = allowRepeat ? 1 : 0;
    the_mesh.savePrefs();
    the_mesh.uiApplyRadioParams();   // live, no reboot
  }
  bool repeaterMode() const override {
    return _node_prefs && _node_prefs->client_repeat != 0;
  }
  float savedRepeatFreq() const override {
    return _node_prefs ? _node_prefs->repeat_saved_freq : 0.0f;
  }
  void setSavedRepeatFreq(float f) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    p->repeat_saved_freq = f;
    the_mesh.savePrefs();
  }
  int repeatFreqCount() const override { return the_mesh.uiRepeatFreqCount(); }
  float repeatFreqMhz(int i) const override {
    return (float)the_mesh.uiRepeatFreqKhz(i) / 1000.0f;
  }
  int16_t tzOffsetMinutes() const override {
    if (!_node_prefs) return 0;
    int16_t fixed = (int16_t)_node_prefs->tz_quarter_hours * 15;
    if (_node_prefs->tz_city_index < 0) return fixed;   // custom: no RTC read
    uint32_t now = epochSeconds();
    uint32_t nowMin = now / 60;
    if (nowMin != _tzCacheMin) {   // recompute DST only when the minute rolls over
      _tzCacheMin = nowMin;
      _tzCacheOff = mishmesh::resolveTzOffset(_node_prefs->tz_city_index, fixed, now);
    }
    return _tzCacheOff;
  }
  void setTzOffsetMinutes(int16_t m) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    if (m < -720) m = -720; else if (m > 840) m = 840;
    p->tz_quarter_hours = (int8_t)(m / 15);
    p->tz_city_index = -1;         // raw offset = custom
    _tzCacheMin = 0xFFFFFFFFu;     // invalidate memo
    the_mesh.savePrefs();
  }
  int tzCityIndex() const override {
    return _node_prefs ? _node_prefs->tz_city_index : -1;
  }
  void setTzCity(int cityIndex) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    if (cityIndex < 0 || cityIndex >= mishmesh::worldCityCount()) return;   // reject garbage
    p->tz_city_index = (int8_t)cityIndex;
    int16_t off = mishmesh::worldCityOffsetNow(cityIndex, epochSeconds());
    if (off < -720) off = -720; else if (off > 840) off = 840;
    p->tz_quarter_hours = (int8_t)(off / 15);   // keep the fixed-offset fallback in sync
    _tzCacheMin = 0xFFFFFFFFu;
    the_mesh.savePrefs();
  }
  bool timeFormat12h() const override {
    return _node_prefs ? _node_prefs->time_fmt_12h != 0 : false;
  }
  void setTimeFormat12h(bool on) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    p->time_fmt_12h = on ? 1 : 0;
    the_mesh.savePrefs();
  }
  bool autoTimeSync() const override {
    return _node_prefs ? _node_prefs->manual_time_set == 0 : true;
  }
  void setAutoTimeSync(bool on) override;     // defined in UITask.cpp (touches sensors)
  void setEpochSeconds(uint32_t secs) override;   // defined in UITask.cpp (touches rtc_clock)
  uint8_t dateFormat() const override {
    return _node_prefs ? _node_prefs->date_format : 0;
  }
  void setDateFormat(uint8_t f) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    p->date_format = (f <= 2) ? f : 0;
    the_mesh.savePrefs();
  }
  uint8_t screenSleepIndex() const override {
    return (uint8_t)mishmesh::screenSleepStoredToIndex(_node_prefs ? _node_prefs->screen_sleep : 0);
  }
  void setScreenSleepIndex(uint8_t idx) override {
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return;
    p->screen_sleep = mishmesh::screenSleepIndexToStored(idx);
    the_mesh.savePrefs();
    if (_host) _host->setAutoOffMillis(mishmesh::screenSleepMillis(idx));   // live
  }
  bool setNodeName(const char* name) override {
    if (!mishmesh::isValidNodeName(name)) return false;
    NodePrefs* p = the_mesh.getNodePrefs();
    if (!p) return false;
    size_t n = strlen(name);
    if (n > sizeof(p->node_name) - 1) n = sizeof(p->node_name) - 1;
    memcpy(p->node_name, name, n);
    p->node_name[n] = 0;
    the_mesh.savePrefs();
    return true;
  }
  bool gpsSupported() const override {
    return _sensors && _sensors->getSettingByKey("gps") != nullptr;
  }
  bool gpsEnabled() const override {
    const char* v = _sensors ? _sensors->getSettingByKey("gps") : nullptr;
    return v && strcmp(v, "1") == 0;
  }
  void setGpsEnabled(bool on) override {
    if (!_sensors) return;
    _sensors->setSettingValue("gps", on ? "1" : "0");
    NodePrefs* p = the_mesh.getNodePrefs();     // persist like ui-tiny does
    if (p) { p->gps_enabled = on ? 1 : 0; the_mesh.savePrefs(); }
  }
  bool gpsHasFix() const override {
    LocationProvider* lp = _sensors ? _sensors->getLocationProvider() : nullptr;
    return lp && gpsEnabled() && lp->isValid();
  }
  int gpsSatellites() const override {
    LocationProvider* lp = _sensors ? _sensors->getLocationProvider() : nullptr;
    return (lp && gpsEnabled()) ? (int)lp->satellitesCount() : 0;
  }
  // [/mishmesh]

  // mishmesh::ContactsService
  int  countByKind(mishmesh::ContactKind k) const override;
  bool getByKind(mishmesh::ContactKind k, int index, mishmesh::ContactView& out) const override;
  int  countFavourites() const override;
  bool getFavourite(int index, mishmesh::ContactView& out) const override;
  // [mishmesh] O(1) raw access + change token backing the contacts list row cache.
  int      contactCount() const override;
  bool     contactAt(int rawIndex, mishmesh::ContactView& out) const override;
  uint32_t contactsSeq() const override;
  bool setFavourite(const uint8_t* pubKey, bool fav) override;
  bool renameContact(const uint8_t* pubKey, const char* name) override;
  uint8_t getTelemetryPerms(const uint8_t* pubKey) const override;
  bool setTelemetryPerm(const uint8_t* pubKey, uint8_t permMask, bool on) override;
  bool getPath(const uint8_t* pubKey, uint8_t* pathOut, uint8_t& encodedLenOut) const override;
  bool setPath(const uint8_t* pubKey, const uint8_t* path, uint8_t encodedLen) override;
  int  countDiscovered() const override;
  bool getDiscovered(int index, mishmesh::ContactView& out) const override;
  bool addDiscovered(const uint8_t* pubKey) override;
  // [mishmesh]
  int  countRecentAdverts() const override;
  bool getRecentAdvert(int index, mishmesh::ContactView& out) const override;
  bool isContact(const uint8_t* pubKey) const override;
  // [/mishmesh]
  bool selfLocation(int32_t& lat1e6, int32_t& lon1e6) const override;
  bool requestTelemetry(const uint8_t* pubKey) override;
  bool resetPath(const uint8_t* pubKey) override;
  bool clearConversation(const uint8_t* pubKey) override;
  bool deleteContact(const uint8_t* pubKey) override;
  uint32_t telemetrySeq() const override;
  bool latestTelemetry(const uint8_t* pubKey, mishmesh::TelemetryView& out) const override;
  bool ping(const uint8_t* pubKey) override;
  uint32_t pingSeq() const override;
  bool latestPing(const uint8_t* pubKey, mishmesh::PingView& out) const override;
  // [mishmesh] room-server login (delegates to the_mesh; result via onRoomLogin)
  bool login(const uint8_t* pubKey, const char* password) override;
  uint32_t loginSeq() const override { return _loginSeq; }
  bool loginResult(const uint8_t* pubKey, bool& ok, bool& isAdmin, uint8_t& perms) const override;
  bool isLoggedIn(const uint8_t* pubKey) const override;
  // [mishmesh] admin CLI command channel (delegates to the_mesh; reply latched in MyMesh)
  bool     sendCliCommand(const uint8_t* pubKey, const char* cmd) override;
  uint32_t cliSeq() const override;
  bool     cliResult(const uint8_t* pubKey, uint32_t afterSeq, bool& ok, const char*& response) const override;
  // [/mishmesh]
  // [mishmesh] repeater status (binary) + login clock
  bool     requestStatus(const uint8_t* pubKey) override;
  uint32_t statusSeq() const override;
  bool     latestStatus(const uint8_t* pubKey, mishmesh::RepeaterStatusView& out) const override;
  uint32_t loginClock(const uint8_t* pubKey) const override;
  // [/mishmesh]
  // [mishmesh] repeater ACL (binary GET_ACCESS_LIST)
  bool     requestAccessList(const uint8_t* pubKey) override;
  uint32_t accessListSeq() const override;
  bool     latestAccessList(const uint8_t* pubKey, mishmesh::AccessListView& out) const override;
  // [/mishmesh]
  // [mishmesh] Ed25519 keygen: delegates to MyMesh::uiMakeIdentityHex (firmware-only).
  bool makeIdentityHex(const char* seedHex, char* out, int outCap) override;
  // [/mishmesh]
  mishmesh::AutoAddConfig getAutoAdd() const override;
  void setAutoAdd(const mishmesh::AutoAddConfig& cfg) override;
  int removeNonChat() override;
  int removeNonFavourites() override;
  int removeAll() override;

  // AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void onRoomLogin(const uint8_t* pubkey, bool success, bool is_admin, uint8_t perms) override;   // [mishmesh]
  void loop() override;

private:
  // [mishmesh] Latest room-login outcome + this-power-cycle logged-in set.
  static const int MM_MAX_LOGINS = 8;
  uint32_t _loginSeq = 0;
  uint8_t  _loginPub[6] = {0};
  bool     _loginOk = false;
  bool     _loginAdmin = false;
  uint8_t  _loginPerms = 0;
  uint8_t  _loggedIn[MM_MAX_LOGINS][6] = {{0}};
  int      _loggedInCount = 0;
  // [/mishmesh]
};
