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

class UITask : public AbstractUITask, public mishmesh::AppServices, public mishmesh::ContactsService {
  DisplayDriver* _display;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  mishmesh::AppletHost* _host;
  mishmesh::HomeApplet* _home;
  mishmesh::AppMenuApplet* _menu;

  mutable uint16_t _batt_mv;        // smoothed; raw ADC reads are noisy
  mutable uint32_t _batt_sampled_at;

  mutable ContactInfo _scratch;     // backs the ContactView returned by getByKind

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

  // mishmesh::ContactsService
  int  countByKind(mishmesh::ContactKind k) const override;
  bool getByKind(mishmesh::ContactKind k, int index, mishmesh::ContactView& out) const override;
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
  int removeAll() override;

  // AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void loop() override;
};
