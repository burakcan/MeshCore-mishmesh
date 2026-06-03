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
  // Shared load/save scratch — load runs once in begin(), save later in loop();
  // never concurrent, so one BSS buffer serves both (saves ~8 KB vs. two).
  static uint8_t _msgIoBuf[mishmesh::ARENA_BYTES + 2048];

  struct MsgSvc : public mishmesh::MessagesService {
    mishmesh::MessageStore* store = nullptr;

    const char* nameFor(const mishmesh::ConvoKey& k) const;
    int  convoCount() const override;
    bool getConvo(int i, mishmesh::ConvoView& out) const override;
    uint16_t totalUnread() const override;
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
  } _msgSvc;
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
  // [/mishmesh]

  // mishmesh::ContactsService
  int  countByKind(mishmesh::ContactKind k) const override;
  bool getByKind(mishmesh::ContactKind k, int index, mishmesh::ContactView& out) const override;
  int  countFavourites() const override;
  bool getFavourite(int index, mishmesh::ContactView& out) const override;
  bool setFavourite(const uint8_t* pubKey, bool fav) override;
  int  countDiscovered() const override;
  bool getDiscovered(int index, mishmesh::ContactView& out) const override;
  bool addDiscovered(const uint8_t* pubKey) override;
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
