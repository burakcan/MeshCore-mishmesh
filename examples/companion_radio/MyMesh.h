#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "AbstractUITask.h"

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE 13

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "6 Jun 2026"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v1.16.0"
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(ESP32)
#include <SPIFFS.h>
#endif

#include "DataStore.h"
#include "NodePrefs.h"

#include <RTClib.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <target.h>

/* ---------------------------------- CONFIGURATION ------------------------------------- */

#ifndef LORA_FREQ
#define LORA_FREQ 915.0
#endif
#ifndef LORA_BW
#define LORA_BW 250
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20
#endif
#ifndef MAX_LORA_TX_POWER
#define MAX_LORA_TX_POWER LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
#define MAX_CONTACTS 100
#endif

#ifndef OFFLINE_QUEUE_SIZE
#define OFFLINE_QUEUE_SIZE 16
#endif

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "MeshCore-"
#endif

#include <helpers/BaseChatMesh.h>
#include <helpers/TransportKeyStore.h>
// [mishmesh]
#include <mishmesh/core/MessageStore.h>
// [/mishmesh]

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS             0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE             0x02
#define REQ_TYPE_GET_TELEMETRY_DATA     0x03

struct AdvertPath {
  uint8_t pubkey_prefix[7];
  uint8_t path_len;
  char    name[32];
  uint32_t recv_timestamp;
  uint8_t path[MAX_PATH_SIZE];
};

class MyMesh : public BaseChatMesh, public DataStoreHost {
public:
  MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui=NULL);

  void begin(bool has_display);
  void startInterface(BaseSerialInterface &serial);

  const char *getNodeName();
  NodePrefs *getNodePrefs();
  uint32_t getBLEPin();

  void loop();
  void handleCmdFrame(size_t len);
  bool advert();
  // [mishmesh]
  // On-device advert trigger for the mishmesh UI. false = zero hop, true = flood
  // routed (default transport scope). Mirrors the CMD_SEND_SELF_ADVERT handler.
  bool sendSelfAdvert(bool flood);
  // [/mishmesh]
  void enterCLIRescue();

  int  getRecentlyHeard(AdvertPath dest[], int max_num);

  // [mishmesh] on-device UI hooks. Additive + grouped for clean upstream merges.
  struct TelemetryLatch {
    uint8_t  pubkey[PUB_KEY_SIZE];
    uint8_t  lpp[MAX_PACKET_PAYLOAD];
    uint8_t  len;
    uint32_t seq;
  };
  struct PingLatch {
    uint8_t  pubkey[PUB_KEY_SIZE];
    uint32_t tag;        // trace tag == send time; matched in onTraceRecv
    uint32_t sentAt;
    uint32_t rttMs;
    uint8_t  hops;
    float    snrUs;      // SNR we received the reply at (their signal to us), dB
    float    snrThem;    // SNR the peer received our ping at (our signal to them), dB
    uint32_t seq;
    bool     replied;
  };
  const TelemetryLatch& uiLastTelemetry() const { return _ui_telemetry; }
  const PingLatch& uiLastPing() const { return _ui_ping; }
  bool uiRequestTelemetry(const uint8_t* pubkey);
  bool uiPing(const uint8_t* pubkey);          // 0-hop trace ("ping"), round-trip latched
  bool uiDeleteContact(const uint8_t* pubkey);
  bool uiClearConversation(const uint8_t* pubkey);
  bool uiSetFavourite(const uint8_t* pubkey, bool fav);   // flags bit0 = favourite
  uint8_t uiGetTelemetryPerms(const uint8_t* pubkey);     // flags bits1-3 (TELEM_PERM_* mask)
  bool uiSetTelemetryPerm(const uint8_t* pubkey, uint8_t perm_mask, bool on);
  bool uiGetPath(const uint8_t* pubkey, uint8_t* out_path, uint8_t& out_path_len);   // false = flood/unknown
  bool uiSetPath(const uint8_t* pubkey, const uint8_t* path, uint8_t path_len);      // 0 hops = reset to flood
  bool uiRenameContact(const uint8_t* pubkey, const char* name);  // local display label
  void uiPersistContacts();
  // [mishmesh] apply current _prefs radio params to the driver live (no reboot),
  // mirroring CMD_SET_RADIO_PARAMS/CMD_SET_RADIO_TX_POWER. Defined in MyMesh.cpp
  // where radio_driver + MAX_LORA_TX_POWER are in scope.
  void uiApplyRadioParams();
  int8_t uiTxPowerMax() const;
  // [/mishmesh]

  // Discovery: adverts seen but not auto-added, so the UI can add them manually.
  static const int UI_MAX_DISCOVERIES = 16;
  int  uiDiscoveryCount();                                // entries not already contacts
  bool uiGetDiscovery(int index, ContactInfo& out);
  bool uiAddDiscovery(const uint8_t* pubkey);             // promote a discovery to a contact
  // [mishmesh] Recent adverts: every advert heard (incl. contacts), newest-first.
  static const int UI_MAX_RECENT_ADVERTS = 16;
  int  uiRecentAdvertCount();
  bool uiGetRecentAdvert(int index, ContactInfo& out);
  // [/mishmesh]
  void uiSetMessageStore(mishmesh::MessageStore* s) { _mm_store = s; }
  DataStore* getStore() const { return _store; }
  // On-device send mirroring CMD_SEND_TXT_MSG: DM (k.type==0) or channel (k.type==1).
  bool mishmeshSendText(const mishmesh::ConvoKey& k, const char* text);
  // Same, but applies a per-chat flood-scope override for this one send (mishmesh
  // per-chat region): scope_key16 = 16-byte TransportKey, or null to fall back to
  // the node default scope. Any session scope set by the companion is saved and
  // restored around the send.
  bool mishmeshSendText(const mishmesh::ConvoKey& k, const char* text, const uint8_t* scope_key16);
  // Seed an (empty) chat for every joined channel (e.g. the default Public
  // channel) so it shows on a fresh device before any message arrives.
  void uiSeedChannels();
  // Auto-retry: re-send an already-stored, still-undelivered DM (identified by
  // its original senderTime). attempt bumps the ACK hash; resetPath floods.
  bool mishmeshRetransmit(const mishmesh::ConvoKey& k, uint32_t senderTime,
                          uint8_t attempt, bool resetPath);
  // [/mishmesh]

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet *packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet *packet) override;
  uint8_t getExtraAckTransmitCount() const override;
  bool filterRecvFloodPacket(mesh::Packet* packet) override;
  bool allowPacketForward(const mesh::Packet* packet) override;

  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis);
  void sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis=0) override;
  void sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis=0) override;

  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
  bool isAutoAddEnabled() const override;
  bool shouldAutoAddContactType(uint8_t type) const override;
  bool shouldOverwriteWhenFull() const override;
  uint8_t getAutoAddMaxHops() const override;
  void onContactsFull() override;
  void onContactOverwrite(const uint8_t* pub_key) override;
  bool onContactPathRecv(ContactInfo& from, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) override;
  void onContactPathUpdated(const ContactInfo &contact) override;
  ContactInfo* processAck(const uint8_t *data) override;
  void queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt, uint32_t sender_timestamp,
                    const uint8_t *extra, int extra_len, const char *text);

  void onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                     const char *text) override;
  void onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                         const char *text) override;
  void onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const uint8_t *sender_prefix, const char *text) override;
  void onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                            const char *text) override;
  void onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                         const uint8_t *data, size_t data_len) override;

  uint8_t onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                           uint8_t len, uint8_t *reply) override;
  void onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) override;
  void onControlDataRecv(mesh::Packet *packet) override;
  void onRawDataRecv(mesh::Packet *packet) override;
  void onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) override;

  uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
  uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;
  void onSendTimeout() override;

  // DataStoreHost methods
  bool onContactLoaded(const ContactInfo& contact) override { return addContact(contact); }
  bool getContactForSave(uint32_t idx, ContactInfo& contact) override { return getContactByIdx(idx, contact); }
  bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) override { return setChannel(channel_idx, ch); }
  bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) override { return getChannel(channel_idx, ch); }

  void clearPendingReqs() {
    pending_login = pending_status = pending_telemetry = pending_discovery = pending_req = 0;
  }

public:
  void savePrefs() { _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon); }

  // [mishmesh] storage stats for the on-device System screen
  uint32_t getStorageUsedKb()  const { return _store->getStorageUsedKb(); }
  uint32_t getStorageTotalKb() const { return _store->getStorageTotalKb(); }
  // [/mishmesh]

#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() {
    sensors.setSettingValue("gps", _prefs.gps_enabled ? "1" : "0");
    if (_prefs.gps_interval > 0) {
      char interval_str[12];  // Max: 24 hours = 86400 seconds (5 digits + null)
      sprintf(interval_str, "%u", _prefs.gps_interval);
      sensors.setSettingValue("gps_interval", interval_str);
    }
  }
#endif

  // To check if there is pending work
  bool hasPendingWork() const;

private:
  void writeOKFrame();
  void writeErrFrame(uint8_t err_code);
  void writeDisabledFrame();
  void writeContactRespFrame(uint8_t code, const ContactInfo &contact);
  void updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len);
  void addToOfflineQueue(const uint8_t frame[], int len);
  int getFromOfflineQueue(uint8_t frame[]);
  int getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) override { 
    return _store->getBlobByKey(key, key_len, dest_buf);
  }
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], int len) override {
    return _store->putBlobByKey(key, key_len, src_buf, len);
  }

  void checkCLIRescueCmd();
  void checkSerialInterface();
  bool isValidClientRepeatFreq(uint32_t f) const;

  // helpers, short-cuts
  void saveChannels() { _store->saveChannels(this); }
  void saveContacts();

  DataStore* _store;
  NodePrefs _prefs;
  uint32_t pending_login;
  uint32_t pending_status;
  uint32_t pending_telemetry, pending_discovery;   // pending _TELEMETRY_REQ
  uint32_t pending_req;   // pending _BINARY_REQ
  BaseSerialInterface *_serial;
  AbstractUITask* _ui;

  ContactsIterator _iter;
  uint32_t _iter_filter_since;
  uint32_t _most_recent_lastmod;
  uint32_t _active_ble_pin;
  bool _iter_started;
  bool _cli_rescue;
  bool send_unscoped;   // force un-scoped flood (instead of using send_scope)
  char cli_command[80];
  uint8_t app_target_ver;
  uint8_t *sign_data;
  uint32_t sign_data_len;
  unsigned long dirty_contacts_expiry;

  // [mishmesh]
  TelemetryLatch _ui_telemetry = {};
  PingLatch _ui_ping = {};
  ContactInfo _ui_discoveries[UI_MAX_DISCOVERIES];
  int _ui_discovery_count = 0;
  void uiNoteDiscovery(const ContactInfo& ci);   // called from onDiscoveredContact
  // [mishmesh]
  ContactInfo _ui_recent_adverts[UI_MAX_RECENT_ADVERTS];
  int _ui_recent_advert_count = 0;
  void uiNoteRecentAdvert(const ContactInfo& ci);   // called from onDiscoveredContact
  // [/mishmesh]
  // Shared by the CMD_* serial handlers and the UI hooks (DRY - see MyMesh.cpp).
  int  startContactRequest(ContactInfo& contact, uint8_t req_type, uint32_t& tag, uint32_t& est_timeout);
  mesh::Packet* startTrace(uint32_t tag, uint32_t auth, uint8_t flags, const uint8_t* path, uint8_t path_len);
  bool removeContactAndBlob(ContactInfo& contact);
  void markContactsDirty();
  mishmesh::MessageStore* _mm_store = nullptr;
  void logRx(mesh::Packet* pkt, int len, float score) override;
  bool mishmeshSendTextImpl(const mishmesh::ConvoKey& k, const char* text);  // send body, scope-agnostic
  // [/mishmesh]

  TransportKey send_scope;

  uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
  uint8_t out_frame[MAX_FRAME_SIZE + 1];
  CayenneLPP telemetry;

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];

    bool isChannelMsg() const;
  };
  int offline_queue_len;
  Frame offline_queue[OFFLINE_QUEUE_SIZE];

  struct AckTableEntry {
    unsigned long msg_sent;
    uint32_t ack;
    ContactInfo* contact;
  };
  #define EXPECTED_ACK_TABLE_SIZE 8
  AckTableEntry expected_ack_table[EXPECTED_ACK_TABLE_SIZE]; // circular table
  int next_ack_idx;

  #define ADVERT_PATH_TABLE_SIZE   16
  AdvertPath advert_paths[ADVERT_PATH_TABLE_SIZE]; // circular table
};

extern MyMesh the_mesh;
