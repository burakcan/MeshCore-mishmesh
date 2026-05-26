#pragma once

#include <stdint.h>

namespace mishmesh {

enum class ContactKind : uint8_t { Chat = 1, Repeater = 2, Room = 3, Sensor = 4 };  // == ADV_TYPE_*

struct ContactView {
  const char*    name;        // valid until the next service call
  uint8_t        type;        // ADV_TYPE_*
  bool           isFavourite;
  bool           hasPath;     // true = direct path known; false = flood
  uint32_t       lastAdvert;  // their clock, UNIX seconds
  const uint8_t* pubKey;      // stable handle for actions
};

struct AutoAddConfig {
  bool    addChat, addRepeater, addRoom, addSensor;
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

// Round-trip result of a ping (status request) to a contact.
struct PingView {
  bool     replied;
  uint32_t rttMs;
};

// The only seam through which applets reach contacts/mesh state. Implemented by
// the companion adapter; faked in host tests.
struct ContactsService {
  virtual ~ContactsService() {}

  virtual int  countByKind(ContactKind k) const = 0;
  virtual bool getByKind(ContactKind k, int index, ContactView& out) const = 0;

  virtual bool requestTelemetry(const uint8_t* pubKey) = 0;
  virtual bool resetPath(const uint8_t* pubKey) = 0;
  virtual bool clearConversation(const uint8_t* pubKey) = 0;
  virtual bool deleteContact(const uint8_t* pubKey) = 0;

  virtual uint32_t telemetrySeq() const = 0;
  virtual bool     latestTelemetry(const uint8_t* pubKey, TelemetryView& out) const = 0;

  virtual bool     ping(const uint8_t* pubKey) = 0;          // status request
  virtual uint32_t pingSeq() const = 0;                      // bumps on each reply
  virtual bool     latestPing(const uint8_t* pubKey, PingView& out) const = 0;

  virtual AutoAddConfig getAutoAdd() const = 0;
  virtual void          setAutoAdd(const AutoAddConfig& cfg) = 0;

  virtual int removeNonChat() = 0;
  virtual int removeAll() = 0;
};

}  // namespace mishmesh
