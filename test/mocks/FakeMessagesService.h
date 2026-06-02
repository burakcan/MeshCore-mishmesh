#pragma once
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/core/MessageStore.h>
#include <vector>
#include <string>
#include <cstdio>

struct FakeMessagesService : mishmesh::MessagesService {
  mishmesh::MessageStore store;
  std::string nameBuf, prevBuf, sndBuf, repName;
  uint32_t deletes = 0;

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
    o.senderTime = r.senderTime; o.localTime = r.localTime; o.senderName = "";
    o.text = self->sndBuf.c_str(); o.status = r.status; o.tripTimeMs = r.tripTimeMs;
    o.heardCount = r.heardCount; o.snrx4 = r.snrx4; o.hops = r.hops; o.path = r.path; o.pathLen = r.pathLen;
    return true;
  }
  void setActiveConvo(const mishmesh::ConvoKey& k) override { store.setActiveConvo(k); }
  void clearActiveConvo() override { store.clearActiveConvo(); }
  int repeatCount(const mishmesh::ConvoKey& k, int m) const override { return store.repeatCount(k, m); }
  bool getRepeat(const mishmesh::ConvoKey& k, int m, int r, mishmesh::RepeatView& o) const override {
    mishmesh::RepeatRec rr; if (!store.getRepeat(k, m, r, rr)) return false;
    auto* self = const_cast<FakeMessagesService*>(this);
    self->repName = "RPT";
    o.repeaterName = self->repName.c_str(); o.knownCount = 1; o.hops = rr.hops; o.snrx4 = rr.snrx4;
    return true;
  }
  bool resolveHop(uint8_t, const char*& name, uint8_t& kc) const override { name = "RPT"; kc = 1; return true; }
  void deleteMessage(const mishmesh::ConvoKey& k, int i) override { store.deleteMessage(k, i); deletes++; }
  void clearConvo(const mishmesh::ConvoKey& k) override { store.clearConvo(k); }
  void deleteConvo(const mishmesh::ConvoKey& k) override { store.deleteConvo(k); }
  void markUnread(const mishmesh::ConvoKey& k) override { store.markUnread(k); }
  uint32_t seq() const override { return store.seq(); }
};
