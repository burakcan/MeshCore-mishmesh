#pragma once

#include <stdint.h>

namespace mishmesh {

enum class ContactKind : uint8_t { Chat = 1, Repeater = 2, Room = 3, Sensor = 4 };  // == ADV_TYPE_*

// Per-contact telemetry permissions: which of our telemetry a contact may pull
// when it asks. Bit values match the firmware's TELEM_PERM_* so the adapter can
// pass them straight through. A bit is honored only when the matching global
// telemetry mode is "allow by per-contact flags".
enum TelemetryPerm : uint8_t {
  TelemPermBase        = 0x01,   // allow telemetry requests at all (battery / base)
  TelemPermLocation    = 0x02,   // include GPS location
  TelemPermEnvironment = 0x04,   // include environment sensors
};

static const int PUBKEY_LEN = 32;   // ContactView::pubKey points to this many bytes
static const int PATH_MAX_BYTES = 64;   // == firmware MAX_PATH_SIZE; out_path buffer size

struct ContactView {
  const char*    name;        // valid until the next service call
  uint8_t        type;        // ADV_TYPE_*
  bool           isFavourite;
  bool           hasPath;     // true = direct path known; false = flood
  uint8_t        hops;        // path length when direct (0 = flood/unknown)
  uint32_t       lastAdvert;  // their clock, UNIX seconds
  const uint8_t* pubKey;      // full PUB_KEY_SIZE key, stable until next call
  bool           hasLocation;
  int32_t        gpsLat;      // degrees * 1e6
  int32_t        gpsLon;
  // [mishmesh] our-clock receive time (UNIX seconds, 0 = unknown); used by the
  // Advert applet's Recent tab for the "heard N ago" column.
  uint32_t       heardAt = 0;
  // [/mishmesh]
};

struct AutoAddConfig {
  bool    autoAddAll;         // master: true = add every advert; false = only the kinds below
  bool    addChat, addRepeater, addRoom, addSensor;   // consulted only when !autoAddAll
  bool    overwriteOldest;
  uint8_t maxHops;            // mirrors prefs; not edited in v1 UI
  bool    notifyWhenFull;     // [mishmesh] on-device "contacts full" alert enable
};

struct TelemetryField {
  uint8_t channel;
  uint8_t type;              // LPP_* (LPPDataHelpers.h)
  float   value, value2, value3;   // GPS: lat/lon/alt; others use value only
};

static const int MAX_TELEM_FIELDS = 20;

struct TelemetryView {
  bool           valid;
  uint8_t        count;
  TelemetryField fields[MAX_TELEM_FIELDS];
};

// Round-trip result of a 0-hop ping (trace) to a contact.
struct PingView {
  bool     replied;
  uint32_t rttMs;
  uint8_t  hops;
  float    snrUs;    // SNR we received the reply at, dB
  float    snrThem;  // SNR the peer received our ping at, dB
};

// A repeater's RepeaterStats surfaced to the UI as plain integers (no companion
// types), decoded from the 56-byte binary status response by the adapter.
struct RepeaterStatusView {
  bool     valid;
  uint16_t battMilliVolts;
  uint16_t txQueueLen;
  int16_t  noiseFloor;      // dBm
  int16_t  lastRssi;        // dBm
  uint32_t packetsRecv, packetsSent;
  uint32_t airTxSecs, airRxSecs;
  uint32_t upTimeSecs;
  uint32_t sentFlood, sentDirect;
  uint32_t recvFlood, recvDirect;
  uint16_t directDups, floodDups;
  uint16_t errEvents;       // the "Debug flags" the user sees
  int16_t  lastSnrX4;       // SNR * 4; divide by 4.0 for dB
  uint32_t recvErrors;
};

// [mishmesh] Repeater ACL (binary REQ_TYPE_GET_ACCESS_LIST). requestAccessList() fires the
// request; the reply bumps accessListSeq(); latestAccessList() decodes it for pubKey.
struct AclEntry { uint8_t pubkey[6]; uint8_t perms; };
static const int MAX_ACL = 20;
struct AccessListView { bool valid; uint8_t count; AclEntry entries[MAX_ACL]; };
// [/mishmesh]

// The only seam through which applets reach contacts/mesh state. Implemented by
// the companion adapter; faked in host tests.
struct ContactsService {
  virtual ~ContactsService() {}

  virtual int  countByKind(ContactKind k) const = 0;
  virtual bool getByKind(ContactKind k, int index, ContactView& out) const = 0;

  // [mishmesh] Cheap raw random access + a change token, so the list UI can build
  // a filtered index once and stop re-scanning the whole table per row per frame.
  // contactAt() is O(1) with no full-record copy; contactsSeq() changes whenever
  // any rendered/ordering field mutates, from any source (advert, phone app,
  // favourite toggle, delete...). Non-pure so other implementers / host fakes keep
  // compiling - a 0 seq + empty count simply disables the cache.
  virtual int      contactCount() const { return 0; }
  virtual bool     contactAt(int rawIndex, ContactView& out) const { (void)rawIndex; (void)out; return false; }
  virtual uint32_t contactsSeq() const { return 0; }
  // [/mishmesh]

  // Favourites span all kinds (flags bit0). The tab is shown only when count > 0.
  virtual int  countFavourites() const = 0;
  virtual bool getFavourite(int index, ContactView& out) const = 0;
  virtual bool setFavourite(const uint8_t* pubKey, bool fav) = 0;
  virtual bool renameContact(const uint8_t* pubKey, const char* name) = 0;   // local display label

  // Discovered nodes: adverts seen but not added as contacts (e.g. auto-add off).
  virtual int  countDiscovered() const = 0;
  virtual bool getDiscovered(int index, ContactView& out) const = 0;
  virtual bool addDiscovered(const uint8_t* pubKey) = 0;   // promote to a real contact

  // [mishmesh] Recent adverts: every advert heard (incl. known contacts), newest
  // first. Non-pure so existing implementers/fakes need not override. isContact
  // lets the Recent row route to the contact vs discover detail screen.
  virtual int  countRecentAdverts() const { return 0; }
  virtual bool getRecentAdvert(int index, ContactView& out) const { (void)index; (void)out; return false; }
  virtual bool isContact(const uint8_t* pubKey) const { (void)pubKey; return false; }
  // [/mishmesh]

  // Our own location (degrees * 1e6); false if unknown.
  virtual bool selfLocation(int32_t& lat1e6, int32_t& lon1e6) const = 0;

  virtual bool requestTelemetry(const uint8_t* pubKey) = 0;

  // Per-contact telemetry permission bits (a TelemetryPerm mask). Non-pure so
  // existing implementers/fakes keep compiling; defaults to "no permissions".
  virtual uint8_t getTelemetryPerms(const uint8_t* pubKey) const { (void)pubKey; return 0; }
  virtual bool setTelemetryPerm(const uint8_t* pubKey, uint8_t permMask, bool on) {
    (void)pubKey; (void)permMask; (void)on; return false;
  }

  virtual bool resetPath(const uint8_t* pubKey) = 0;

  // Manual outbound routing path. encodedLen is the firmware out_path_len byte:
  // top 2 bits = hashSize-1 (1..3 bytes/hop), low 6 bits = hop count. `path` holds
  // hopCount*hashSize raw hash bytes (leading bytes of each repeater's key). A path
  // with 0 hops resets the contact to flood. pathOut must hold PATH_MAX_BYTES.
  // Non-pure so existing implementers/fakes keep compiling.
  virtual bool getPath(const uint8_t* pubKey, uint8_t* pathOut, uint8_t& encodedLenOut) const {
    (void)pubKey; (void)pathOut; (void)encodedLenOut; return false;
  }
  virtual bool setPath(const uint8_t* pubKey, const uint8_t* path, uint8_t encodedLen) {
    (void)pubKey; (void)path; (void)encodedLen; return false;
  }
  virtual bool clearConversation(const uint8_t* pubKey) = 0;
  virtual bool deleteContact(const uint8_t* pubKey) = 0;

  virtual uint32_t telemetrySeq() const = 0;
  virtual bool     latestTelemetry(const uint8_t* pubKey, TelemetryView& out) const = 0;

  virtual bool     ping(const uint8_t* pubKey) = 0;          // status request
  virtual uint32_t pingSeq() const = 0;                      // bumps on each reply
  virtual bool     latestPing(const uint8_t* pubKey, PingView& out) const = 0;

  // [mishmesh] Room-server login. login() sends `password` to a Room contact; the
  // result arrives asynchronously (server round-trip) and bumps loginSeq().
  // loginResult() reports the most recent outcome. isLoggedIn() tracks contacts
  // logged in during this power cycle so a re-open can skip the prompt. Non-pure
  // so other implementers / host fakes keep compiling.
  virtual bool     login(const uint8_t* pubKey, const char* password) { (void)pubKey; (void)password; return false; }
  virtual uint32_t loginSeq() const { return 0; }
  virtual bool     loginResult(const uint8_t* pubKey, bool& ok, bool& isAdmin, uint8_t& perms) const {
    (void)pubKey; (void)ok; (void)isAdmin; (void)perms; return false;
  }
  virtual bool     isLoggedIn(const uint8_t* pubKey) const { (void)pubKey; return false; }
  // [/mishmesh]

  // [mishmesh] Admin CLI command to a logged-in server (repeater/room). sendCliCommand()
  // sends a text command; the reply arrives asynchronously (server round-trip) and bumps
  // cliSeq(). cliResult() reports the outcome for pubKey - false when no reply is latched
  // for this contact (e.g. a reply for a different one). `response` points at adapter-owned
  // storage valid until the next reply; consumers copy what they keep. `ok` is reserved for
  // a future reject signal and is currently always true when a reply is present. Non-pure so
  // other implementers / host fakes keep compiling.
  virtual bool     sendCliCommand(const uint8_t* pubKey, const char* cmd) { (void)pubKey; (void)cmd; return false; }
  virtual uint32_t cliSeq() const { return 0; }
  // afterSeq: report a reply only if it is newer than this (the cliSeq() captured when the
  // request was fired) - so a stale earlier reply for the same contact is not mistaken for
  // this one. Returns false if no newer reply for pubKey is latched. `response` points at
  // adapter-owned storage valid until the next reply; consumers copy what they keep.
  virtual bool     cliResult(const uint8_t* pubKey, uint32_t afterSeq, bool& ok, const char*& response) const {
    (void)pubKey; (void)afterSeq; (void)ok; (void)response; return false;
  }
  // [/mishmesh]

  // [mishmesh] Repeater status (binary REQ_TYPE_GET_STATUS). requestStatus() fires the
  // request; the reply arrives asynchronously and bumps statusSeq(). latestStatus()
  // decodes it for pubKey (false when none is latched for this contact). loginClock()
  // is the repeater's UNIX-seconds clock captured from the login response (0 = unknown).
  virtual bool     requestStatus(const uint8_t* pubKey) { (void)pubKey; return false; }
  virtual uint32_t statusSeq() const { return 0; }
  virtual bool     latestStatus(const uint8_t* pubKey, RepeaterStatusView& out) const {
    (void)pubKey; (void)out; return false;
  }
  virtual uint32_t loginClock(const uint8_t* pubKey) const { (void)pubKey; return 0; }

  // [mishmesh] Repeater ACL - see AccessListView above.
  virtual bool     requestAccessList(const uint8_t* pubKey) { (void)pubKey; return false; }
  virtual uint32_t accessListSeq() const { return 0; }
  virtual bool     latestAccessList(const uint8_t* pubKey, AccessListView& out) const {
    (void)pubKey; out.valid = false; return false;
  }
  // [/mishmesh]

  // [mishmesh] Key generation: produce a 64-byte Ed25519 private key as 128 hex.
  // seedHex null/empty => random (device RNG); else seedHex is a 64-hex (32-byte)
  // seed expanded via ed25519_create_keypair. Returns false on bad seed / no crypto.
  // out must hold >= 129 bytes.
  virtual bool makeIdentityHex(const char* seedHex, char* out, int outCap) {
    (void)seedHex; (void)out; (void)outCap; return false;
  }
  // [/mishmesh]

  virtual AutoAddConfig getAutoAdd() const = 0;
  virtual void          setAutoAdd(const AutoAddConfig& cfg) = 0;

  virtual int removeNonChat() = 0;
  virtual int removeNonFavourites() = 0;
  virtual int removeAll() = 0;
};

}  // namespace mishmesh
