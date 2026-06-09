#pragma once
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/core/MessageStore.h>
#include <vector>
#include <string>
#include <map>
#include <cstdio>
#include <cstring>

struct FakeMessagesService : mishmesh::MessagesService {
  mishmesh::MessageStore store;
  std::string nameBuf, prevBuf, sndBuf;
  std::string senderName;   // surfaced from getMessage (default "" = no sender)
  uint32_t deletes = 0;
  std::string lastSent;
  mishmesh::ConvoKey lastSentKey{};
  int sends = 0;

  enum ChanOp { OP_NONE, OP_CREATE_PRIV, OP_JOIN_PRIV, OP_JOIN_PUB, OP_JOIN_HASH };
  ChanOp lastChanOp = OP_NONE;
  std::string lastChanName, lastChanKey;
  int chanCalls = 0;
  mishmesh::ChanResult chanResult = mishmesh::ChanResult::Ok;
  bool publicJoined = false;

  static std::string keyName(const mishmesh::ConvoKey& k) {
    char b[24];
    if (k.type == 1) snprintf(b, sizeof(b), "#chan%u", k.id[0]);
    else snprintf(b, sizeof(b), "%02X%02X", k.id[0], k.id[1]);
    return b;
  }
  int convoCount() const override { return store.convoCount(); }
  bool getConvo(int i, mishmesh::ConvoView& o) const override {
    mishmesh::ConvoSummary c; if (!store.getConvo(i, c)) return false;
    auto* self = const_cast<FakeMessagesService*>(this);
    self->nameBuf = keyName(c.key);
    self->prevBuf = "...";
    o.key = c.key; o.isChannel = c.key.type == 1; o.name = self->nameBuf.c_str();
    o.lastTime = c.lastTime; o.unread = c.unread; o.preview = self->prevBuf.c_str();
    return true;
  }
  uint16_t totalUnread() const override { return store.totalUnread(); }
  int messageCount(const mishmesh::ConvoKey& k) const override { return store.messageCount(k); }
  bool getMessage(const mishmesh::ConvoKey& k, int i, mishmesh::MessageView& o) const override {
    mishmesh::MsgRecord r; if (!store.getMessage(k, i, r)) return false;
    auto* self = const_cast<FakeMessagesService*>(this);
    self->sndBuf.assign(r.text, r.textLen);
    o.outbound = r.kind != mishmesh::KIND_INBOUND; o.isChannel = k.type == 1; o.kind = r.kind;
    o.senderTime = r.senderTime; o.localTime = r.localTime; o.senderName = self->senderName.c_str();
    o.text = self->sndBuf.c_str(); o.status = r.status; o.tripTimeMs = r.tripTimeMs;
    o.heardCount = r.heardCount; o.snrx4 = r.snrx4; o.hops = r.hops; o.path = r.path; o.pathLen = r.pathLen;
    return true;
  }
  void setActiveConvo(const mishmesh::ConvoKey& k) override { store.setActiveConvo(k); }
  void clearActiveConvo() override { store.clearActiveConvo(); }
  int repeatCount(const mishmesh::ConvoKey& k, int m) const override { return store.repeatCount(k, m); }
  bool getRepeat(const mishmesh::ConvoKey& k, int m, int r, mishmesh::RepeatView& o) const override {
    mishmesh::RepeatRec rr; if (!store.getRepeat(k, m, r, rr)) return false;
    o.hops = rr.hops; o.snrx4 = rr.snrx4; o.path = rr.path; o.pathLen = rr.pathLen;
    return true;
  }
  bool resolveHop(uint8_t b, const char*& name, uint8_t& kc) const override {
    if (b == 0xAA) { name = "Alice"; kc = 1; return true; }   // everything else falls back to hex
    name = ""; kc = 0; return false;
  }
  void deleteMessage(const mishmesh::ConvoKey& k, int i) override { store.deleteMessage(k, i); deletes++; }
  void clearConvo(const mishmesh::ConvoKey& k) override { store.clearConvo(k); }
  void deleteConvo(const mishmesh::ConvoKey& k) override { store.deleteConvo(k); }
  void markUnread(const mishmesh::ConvoKey& k) override { store.markUnread(k); }
  std::map<std::string, std::string> regions;   // per-chat region store
  int region(const mishmesh::ConvoKey& k, char* dst, int cap) const override {
    if (dst && cap > 0) dst[0] = 0;
    auto it = regions.find(keyName(k));
    if (it == regions.end() || !dst || cap <= 1) return 0;
    std::strncpy(dst, it->second.c_str(), cap - 1); dst[cap - 1] = 0;
    return (int)std::strlen(dst);
  }
  void setRegion(const mishmesh::ConvoKey& k, const char* name) override {
    if (name && name[0]) regions[keyName(k)] = name;
    else regions.erase(keyName(k));
  }
  bool sendText(const mishmesh::ConvoKey& k, const char* text) override {
    lastSent = text ? text : "";
    lastSentKey = k;
    sends++;
    uint16_t len = (uint16_t)lastSent.size();
    if (k.type == 1) store.appendOutboundChannel(k, lastSent.c_str(), len, 0, 0);
    else             store.appendOutboundDM(k, lastSent.c_str(), len, 0, 0, 0, 0);
    return true;
  }
  mishmesh::ChanResult createPrivateChannel(const char* name) override {
    lastChanOp = OP_CREATE_PRIV; lastChanName = name ? name : ""; chanCalls++; return chanResult;
  }
  mishmesh::ChanResult joinPrivateChannel(const char* name, const char* keyHex) override {
    lastChanOp = OP_JOIN_PRIV; lastChanName = name ? name : ""; lastChanKey = keyHex ? keyHex : "";
    chanCalls++; return chanResult;
  }
  mishmesh::ChanResult joinPublicChannel() override {
    lastChanOp = OP_JOIN_PUB; chanCalls++; return chanResult;
  }
  mishmesh::ChanResult joinHashtagChannel(const char* hashtag) override {
    lastChanOp = OP_JOIN_HASH; lastChanName = hashtag ? hashtag : ""; chanCalls++; return chanResult;
  }
  bool publicChannelJoined() const override { return publicJoined; }
  uint32_t seq() const override { return store.seq(); }
};
