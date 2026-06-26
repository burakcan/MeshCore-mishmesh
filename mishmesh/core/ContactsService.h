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

// The only seam through which applets reach contacts/mesh state. Implemented by
// the companion adapter; faked in host tests.
struct ContactsService {
  virtual ~ContactsService() {}

  virtual int  countByKind(ContactKind k) const = 0;
  virtual bool getByKind(ContactKind k, int index, ContactView& out) const = 0;

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

  virtual AutoAddConfig getAutoAdd() const = 0;
  virtual void          setAutoAdd(const AutoAddConfig& cfg) = 0;

  virtual int removeNonChat() = 0;
  virtual int removeNonFavourites() = 0;
  virtual int removeAll() = 0;
};

}  // namespace mishmesh
