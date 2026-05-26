#pragma once

#include <mishmesh/core/ContactsService.h>
#include <cstring>
#include <vector>

// In-memory ContactsService for host tests. Holds a small set of fake contacts
// per kind and records action calls.
class FakeContactsService : public mishmesh::ContactsService {
public:
  struct Row { std::string name; uint8_t pubkey[6]; bool favourite=false; bool hasPath=true; uint32_t lastAdvert=0; };
  std::vector<Row> chats, repeaters, rooms, sensors;

  mishmesh::AutoAddConfig cfg{true,false,false,false,false,3};
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
    out.hasPath = r.hasPath; out.lastAdvert = r.lastAdvert; out.pubKey = r.pubkey;
    return true;
  }
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
  int removeAll() override { calls.push_back("removeall"); int n=(int)(chats.size()+repeaters.size()+rooms.size()+sensors.size()); chats.clear();repeaters.clear();rooms.clear();sensors.clear(); return n; }
};
