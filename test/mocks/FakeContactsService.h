#pragma once

#include <mishmesh/core/ContactsService.h>
#include <cstring>
#include <vector>

// In-memory ContactsService for host tests. Holds a small set of fake contacts
// per kind and records action calls.
class FakeContactsService : public mishmesh::ContactsService {
public:
  struct Row { std::string name; uint8_t pubkey[6]; bool favourite=false; bool hasPath=true; uint32_t lastAdvert=0;
               bool hasLocation=false; int32_t gpsLat=0, gpsLon=0; uint8_t type=1; };
  std::vector<Row> chats, repeaters, rooms, sensors, discovered;
  bool hasSelfLoc=false; int32_t selfLat=0, selfLon=0;

  mishmesh::AutoAddConfig cfg{true,false,false,false,false,false,3};  // autoAddAll, 4 kinds, overwrite, maxHops
  uint32_t telemSeq = 0;
  mishmesh::TelemetryView telem{};

  // Action log
  std::vector<std::string> calls;
  std::string lastDeleted, lastTelemetryReq, lastResetPath, lastCleared, lastPing;
  uint32_t pingSeqVal = 0;
  mishmesh::PingView pingResult{false, 0};

  const std::vector<Row>& list(mishmesh::ContactKind k) const {
    switch (k) {
      case mishmesh::ContactKind::Chat: return chats;
      case mishmesh::ContactKind::Repeater: return repeaters;
      case mishmesh::ContactKind::Room: return rooms;
      default: return sensors;
    }
  }
  static std::string keyStr(const uint8_t* pk) { return std::string((const char*)pk, 6); }

  int countByKind(mishmesh::ContactKind k) const override { return (int)list(k).size(); }
  bool getByKind(mishmesh::ContactKind k, int i, mishmesh::ContactView& out) const override {
    const auto& l = list(k);
    if (i < 0 || i >= (int)l.size()) return false;
    const Row& r = l[i];
    out.name = r.name.c_str(); out.type = (uint8_t)k; out.isFavourite = r.favourite;
    out.hasPath = r.hasPath; out.hops = r.hasPath ? 1 : 0; out.lastAdvert = r.lastAdvert; out.pubKey = r.pubkey;
    out.hasLocation = r.hasLocation; out.gpsLat = r.gpsLat; out.gpsLon = r.gpsLon;
    return true;
  }
  int countFavourites() const override {
    int n = 0;
    for (auto* l : {&chats, &repeaters, &rooms, &sensors})
      for (const Row& r : *l) if (r.favourite) n++;
    return n;
  }
  bool getFavourite(int index, mishmesh::ContactView& out) const override {
    int seen = 0;
    const mishmesh::ContactKind kinds[4] = {mishmesh::ContactKind::Chat, mishmesh::ContactKind::Repeater,
                                            mishmesh::ContactKind::Room, mishmesh::ContactKind::Sensor};
    for (mishmesh::ContactKind k : kinds) {
      const auto& l = list(k);
      for (int i = 0; i < (int)l.size(); i++) {
        if (!l[i].favourite) continue;
        if (seen == index) return getByKind(k, i, out);
        seen++;
      }
    }
    return false;
  }
  bool setFavourite(const uint8_t* pk, bool fav) override {
    for (auto* l : {&chats, &repeaters, &rooms, &sensors})
      for (Row& r : *const_cast<std::vector<Row>*>(l))
        if (keyStr(r.pubkey) == keyStr(pk)) { r.favourite = fav; calls.push_back("setfav"); return true; }
    return false;
  }
  std::string lastRenamed;   // pubkey of the last renamed contact
  bool renameContact(const uint8_t* pk, const char* name) override {
    if (!name || !name[0]) return false;
    for (auto* l : {&chats, &repeaters, &rooms, &sensors})
      for (Row& r : *const_cast<std::vector<Row>*>(l))
        if (keyStr(r.pubkey) == keyStr(pk)) { r.name = name; lastRenamed = keyStr(pk); calls.push_back("rename"); return true; }
    return false;
  }
  int countDiscovered() const override { return (int)discovered.size(); }
  bool getDiscovered(int index, mishmesh::ContactView& out) const override {
    if (index < 0 || index >= (int)discovered.size()) return false;
    const Row& r = discovered[index];
    out.name = r.name.c_str(); out.type = r.type; out.isFavourite = false;
    out.hasPath = r.hasPath; out.hops = r.hasPath ? 1 : 0; out.lastAdvert = r.lastAdvert; out.pubKey = r.pubkey;
    out.hasLocation = r.hasLocation; out.gpsLat = r.gpsLat; out.gpsLon = r.gpsLon;
    return true;
  }
  bool addDiscovered(const uint8_t* pk) override {
    for (size_t i = 0; i < discovered.size(); i++) {
      if (keyStr(discovered[i].pubkey) != keyStr(pk)) continue;
      Row r = discovered[i];
      discovered.erase(discovered.begin() + i);
      switch ((mishmesh::ContactKind)r.type) {
        case mishmesh::ContactKind::Repeater: repeaters.push_back(r); break;
        case mishmesh::ContactKind::Room:     rooms.push_back(r); break;
        case mishmesh::ContactKind::Sensor:   sensors.push_back(r); break;
        default:                              chats.push_back(r); break;
      }
      calls.push_back("adddiscovered");
      return true;
    }
    return false;
  }
  bool selfLocation(int32_t& lat, int32_t& lon) const override { lat = selfLat; lon = selfLon; return hasSelfLoc; }
  bool requestTelemetry(const uint8_t* pk) override { calls.push_back("telem"); lastTelemetryReq = keyStr(pk); return true; }
  bool resetPath(const uint8_t* pk) override { calls.push_back("resetpath"); lastResetPath = keyStr(pk); return true; }
  bool clearConversation(const uint8_t* pk) override { calls.push_back("clear"); lastCleared = keyStr(pk); return true; }
  bool deleteContact(const uint8_t* pk) override { calls.push_back("delete"); lastDeleted = keyStr(pk); return true; }
  uint32_t telemetrySeq() const override { return telemSeq; }
  bool latestTelemetry(const uint8_t*, mishmesh::TelemetryView& out) const override { out = telem; return telem.valid; }
  bool ping(const uint8_t* pk) override { calls.push_back("ping"); lastPing = keyStr(pk); return true; }
  uint32_t pingSeq() const override { return pingSeqVal; }
  bool latestPing(const uint8_t*, mishmesh::PingView& out) const override { out = pingResult; return true; }
  mishmesh::AutoAddConfig getAutoAdd() const override { return cfg; }
  void setAutoAdd(const mishmesh::AutoAddConfig& c) override { cfg = c; calls.push_back("setautoadd"); }
  int removeNonChat() override { calls.push_back("removenonchat"); int n=(int)(repeaters.size()+rooms.size()+sensors.size()); repeaters.clear();rooms.clear();sensors.clear(); return n; }
  int removeNonFavourites() override {
    calls.push_back("removenonfav");
    int removed = 0;
    for (auto* l : {&chats, &repeaters, &rooms, &sensors})
      for (size_t i = 0; i < l->size(); )
        if (!(*l)[i].favourite) { l->erase(l->begin() + i); removed++; } else i++;
    return removed;
  }
  int removeAll() override { calls.push_back("removeall"); int n=(int)(chats.size()+repeaters.size()+rooms.size()+sensors.size()); chats.clear();repeaters.clear();rooms.clear();sensors.clear(); return n; }
};
