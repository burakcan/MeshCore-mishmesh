#pragma once

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
// [mishmesh]
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>
// [/mishmesh]

class UITask : public AbstractUITask, public mishmesh::AppServices, public mishmesh::ContactsService {
  DisplayDriver* _display;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  mishmesh::AppletHost* _host;
  mishmesh::HomeApplet* _home;
  mishmesh::AppMenuApplet* _menu;

  mutable uint16_t _batt_mv;        // smoothed; raw ADC reads are noisy
  mutable uint32_t _batt_sampled_at;
  mutable uint32_t _heap_min = 0;   // free-heap low watermark; nRF52 has no built-in one

  mutable ContactInfo _scratch;     // backs the ContactView returned by getByKind

  static void fillView(const ContactInfo& c, mishmesh::ContactView& out);

  // [mishmesh]
  mishmesh::MessageStore _msgStore;
  uint32_t _msgDirtySeq = 0;
  uint32_t _msgFlushAt  = 0;
  uint32_t _msgDirtySince = 0;   // when the store first went dirty; caps flush deferral
  // Shared load/save scratch — load runs once in begin(), save later in loop();
  // never concurrent, so one BSS buffer serves both (saves ~8 KB vs. two).
  static uint8_t _msgIoBuf[mishmesh::ARENA_BYTES + 2048];

  struct MsgSvc : public mishmesh::MessagesService {
    mishmesh::MessageStore* store = nullptr;
    mishmesh::AppletStorage* storage = nullptr;   // per-chat region persistence

    const char* nameFor(const mishmesh::ConvoKey& k) const;
    int  convoCount() const override;
    bool getConvo(int i, mishmesh::ConvoView& out) const override;
    uint16_t totalUnread() const override;
    uint16_t totalNotifyUnread() const override;
    int  messageCount(const mishmesh::ConvoKey& k) const override;
    bool getMessage(const mishmesh::ConvoKey& k, int i, mishmesh::MessageView& out) const override;
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
    mishmesh::ChanResult createPrivateChannel(const char* name) override;
    mishmesh::ChanResult joinPrivateChannel(const char* name, const char* keyHex) override;
    mishmesh::ChanResult joinPublicChannel() override;
    mishmesh::ChanResult joinHashtagChannel(const char* hashtag) override;
    bool publicChannelJoined() const override;
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
  mishmesh::sound::SoundEngine _sound;

  // Notification routing is deferred from notify() to the next loop(): notify()
  // fires from a mesh recv callback that runs BEFORE MyMesh appends the message to
  // the store, so the store (unread totals, latest message) is only fresh one step
  // later. loop() drains this and dispatches against the up-to-date store.
  bool        _notifyPending = false;
  UIEventType _notifyEvent = UIEventType::none;
  void dispatchNotification(UIEventType t);
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
        _host(nullptr), _home(nullptr), _menu(nullptr),
        _batt_mv(0), _batt_sampled_at(0) {
#ifdef UI_HAS_JOYSTICK
    _joystick = nullptr;
    _backBtn = nullptr;
#else
    _userBtn = nullptr;
#endif
  }

  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);

  // mishmesh::AppServices
  const char* nodeName() const override { return _node_prefs ? _node_prefs->node_name : ""; }
  uint16_t batteryMillivolts() const override;
  uint32_t epochSeconds() const override;
  bool systemStats(mishmesh::SystemStats& out) const override;
  // [mishmesh]
  bool bleSupported() const override { return blePin() != 0 || isSerialEnabled(); }
  bool bleEnabled()   const override { return isSerialEnabled(); }
  bool bleConnected() const override { return hasConnection(); }
  uint32_t blePin()   const override { return the_mesh.getBLEPin(); }
  void setBleEnabled(bool on) override { if (on) enableSerial(); else disableSerial(); }
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
  // [/mishmesh]

  // mishmesh::ContactsService
  int  countByKind(mishmesh::ContactKind k) const override;
  bool getByKind(mishmesh::ContactKind k, int index, mishmesh::ContactView& out) const override;
  int  countFavourites() const override;
  bool getFavourite(int index, mishmesh::ContactView& out) const override;
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
  mishmesh::AutoAddConfig getAutoAdd() const override;
  void setAutoAdd(const mishmesh::AutoAddConfig& cfg) override;
  int removeNonChat() override;
  int removeNonFavourites() override;
  int removeAll() override;

  // AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void loop() override;
};
