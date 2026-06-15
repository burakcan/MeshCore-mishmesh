// mishmesh/core/MessageStore.cpp
#include "MessageStore.h"
namespace mishmesh {

ConvoKey directKey(const uint8_t* p)   { ConvoKey k; k.type = 0; memcpy(k.id, p, 6); return k; }
ConvoKey channelKey(uint8_t idx)       { ConvoKey k; k.type = 1; memset(k.id, 0, 6); k.id[0] = idx; return k; }

void MessageStore::reset() {
  _used = 0; _deadBytes = 0; _seq = 0; _convoCount = 0;
  _hasActive = false;
  _hasLastInbound = false;
  memset(_tracked, 0, sizeof(_tracked));
}

// --- record field helpers ---
static const int REC_HDR = 17;   // flags(1)+type(1)+id(6)+senderTime(4)+localTime(4)+textLen(1)
static uint8_t  recKind(const uint8_t* r)     { return (r[0] >> 3) & 0x03; }
static bool     recDead(const uint8_t* r)     { return r[0] & 0x04; }
static uint16_t recTextLen(const uint8_t* r)  { return r[16]; }

static void rdKey(const uint8_t* r, ConvoKey& k) { k.type = r[1]; memcpy(k.id, r + 2, 6); }
static uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void     wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }

uint32_t MessageStore::recordSize(const uint8_t* r) const {
  uint32_t n = REC_HDR + recTextLen(r);
  switch (recKind(r)) {
    case KIND_INBOUND:  n += 2 + r[n + 1]; break;      // snrx4(1)+pathLen(1)+path
    case KIND_OUT_DM:   n += 3; break;                  // status(1)+trip(2)
    case KIND_OUT_CHAN: n += 2; break;                  // status(1)+heard(1)
  }
  return n;
}

int MessageStore::findConvo(const ConvoKey& key) const {
  for (int i = 0; i < _convoCount; i++) if (_convos[i].key.equals(key)) return i;
  return -1;
}

int MessageStore::ensureConvo(const ConvoKey& key) {
  int ci = findConvo(key);
  if (ci >= 0) return ci;
  if (_convoCount >= MAX_CONVOS) {
    // evict the least-recently-active convo to make room
    int lru = 0;
    for (int i = 1; i < _convoCount; i++)
      if (_convos[i].lastTime < _convos[lru].lastTime) lru = i;
    ConvoKey victim = _convos[lru].key;
    while (_convos[lru].count > 0) {
      tombstoneOldestOf(victim);
      lru = findConvo(victim);
      if (lru < 0) break;
    }
    if (lru >= 0) dropConvoIfEmpty(lru);
    // _convoCount is now < MAX_CONVOS; fall through to allocate the new slot
  }
  ci = _convoCount++;
  _convos[ci].key = key; _convos[ci].lastTime = 0; _convos[ci].unread = 0;
  _convos[ci].notifyUnread = 0; _convos[ci].count = 0;
  return ci;
}

void MessageStore::touchConvo(int ci, uint32_t time) {
  // lastTime drives recency sort (getConvo) and LRU eviction — it is never shown.
  // Make the touched convo always become the newest: use the real timestamp when
  // it's actually ahead, else one past the current maximum. This keeps ordering by
  // true receive order even when the RTC is unset (time==0) or senders' clocks are
  // skewed (a per-sender senderTime would otherwise misorder the list).
  uint32_t maxT = 0;
  for (int i = 0; i < _convoCount; i++) if (_convos[i].lastTime > maxT) maxT = _convos[i].lastTime;
  uint32_t nt = time > maxT ? time : maxT + 1;
  if (nt > _convos[ci].lastTime) _convos[ci].lastTime = nt;
}

// writes a header + text into _arena at _used; returns offset, advances _used.
// trailerLen bytes are left zeroed after the text for the caller to fill.
static uint32_t writeHeader(uint8_t* arena, uint32_t& used, uint8_t kind, bool isChan,
                            const ConvoKey& key, const char* text, uint16_t textLen,
                            uint32_t senderTime, uint32_t localTime) {
  uint32_t off = used;
  uint8_t* r = arena + off;
  r[0] = (uint8_t)((kind & 0x03) << 3) | (isChan ? 0x02 : 0) | (kind != KIND_INBOUND ? 0x01 : 0);
  r[1] = key.type; memcpy(r + 2, key.id, 6);
  wr32(r + 8, senderTime); wr32(r + 12, localTime);
  if (textLen > MAX_TEXT) textLen = MAX_TEXT;
  r[16] = (uint8_t)textLen; memcpy(r + 17, text, textLen);
  used += REC_HDR + textLen;
  return off;
}

void MessageStore::appendInbound(const ConvoKey& key, const char* text, uint16_t textLen,
                                 uint32_t senderTime, uint32_t recvTime,
                                 int8_t snrx4, const uint8_t* path, uint8_t pathLen) {
  if (textLen > MAX_TEXT) textLen = MAX_TEXT;
  if (pathLen > MAX_PATH) pathLen = MAX_PATH;
  uint32_t need = REC_HDR + textLen + 2 + pathLen;
  if (!reserve(need)) return;                       // Task 4 makes reserve real; Task1 stub returns true
  int ci = ensureConvo(key);
  if (ci < 0) return;
  writeHeader(_arena, _used, KIND_INBOUND, key.type == 1, key, text, textLen, senderTime, recvTime);
  _arena[_used++] = (uint8_t)snrx4;
  _arena[_used++] = pathLen;
  if (pathLen) { memcpy(_arena + _used, path, pathLen); _used += pathLen; }
  _convos[ci].count++;
  if (_convos[ci].count > PER_CHAT_CAP) tombstoneOldestOf(key);
  if (!(_hasActive && _active.equals(key))) _convos[ci].unread++;
  touchConvo(ci, recvTime ? recvTime : senderTime);
  _lastInbound = key; _hasLastInbound = true;   // subject of the next notification
  _seq++;
}

int MessageStore::messageCount(const ConvoKey& key) const {
  int n = 0;
  for (uint32_t off = 0; off < _used; ) {
    const uint8_t* r = _arena + off;
    uint32_t sz = recordSize(r);
    if (!recDead(r)) { ConvoKey k; rdKey(r, k); if (k.equals(key)) n++; }
    off += sz;
  }
  return n;
}

bool MessageStore::getMessage(const ConvoKey& key, int index, MsgRecord& out) const {
  int n = 0;
  for (uint32_t off = 0; off < _used; ) {
    const uint8_t* r = _arena + off;
    uint32_t sz = recordSize(r);
    if (!recDead(r)) {
      ConvoKey k; rdKey(r, k);
      if (k.equals(key)) {
        if (n == index) {
          out.key = k; out.kind = recKind(r);
          out.senderTime = rd32(r + 8); out.localTime = rd32(r + 12);
          out.textLen = recTextLen(r); out.text = (const char*)(r + 17);
          out.snrx4 = 0; out.hops = 0; out.path = nullptr; out.pathLen = 0;
          out.status = ST_PENDING; out.tripTimeMs = 0; out.heardCount = 0;
          const uint8_t* t = r + REC_HDR + out.textLen;
          if (out.kind == KIND_INBOUND) {
            out.snrx4 = (int8_t)t[0]; out.pathLen = t[1]; out.hops = t[1];
            out.path = t + 2;
          } else if (out.kind == KIND_OUT_DM) {
            out.status = t[0]; memcpy(&out.tripTimeMs, t + 1, 2);
          } else { out.status = t[0]; out.heardCount = t[1]; }
          return true;
        }
        n++;
      }
    }
    off += sz;
  }
  return false;
}

bool MessageStore::getConvo(int index, ConvoSummary& out) const {
  if (index < 0 || index >= _convoCount) return false;
  // selection-rank by lastTime descending without mutating _convos
  int rank = 0, best = -1; uint32_t bestTime = 0; bool bestSet = false;
  for (int pass = 0; pass <= index; pass++) {
    best = -1; bestSet = false;
    for (int i = 0; i < _convoCount; i++) {
      uint32_t t = _convos[i].lastTime;
      bool after = false;
      // pick the i with the (pass+1)-th largest lastTime, tie-break by index
      (void)after;
      if (!bestSet || t > bestTime || (t == bestTime && i > best)) {
        // skip ones already chosen in earlier passes
        bool chosen = false;
        // count how many have strictly greater time, or equal-time-with-greater-index
        int greater = 0;
        for (int j = 0; j < _convoCount; j++) {
          if (j == i) continue;
          uint32_t tj = _convos[j].lastTime;
          if (tj > t || (tj == t && j > i)) greater++;
        }
        if (greater == pass) { best = i; bestTime = t; bestSet = true; }
      }
    }
  }
  if (best < 0) return false;
  out = _convos[best];
  return true;
}

uint16_t MessageStore::totalUnread() const {
  uint32_t t = 0;
  for (int i = 0; i < _convoCount; i++) t += _convos[i].unread;
  return t > 0xFFFF ? 0xFFFF : (uint16_t)t;
}

int  MessageStore::convoCount() const { return _convoCount; }
void MessageStore::setActiveConvo(const ConvoKey& key) {
  _active = key; _hasActive = true;
  int ci = findConvo(key);
  if (ci >= 0 && (_convos[ci].unread || _convos[ci].notifyUnread)) {
    _convos[ci].unread = 0; _convos[ci].notifyUnread = 0; _seq++;
  }
}
void MessageStore::clearActiveConvo() { _hasActive = false; }
bool MessageStore::activeConvo(ConvoKey& out) const {
  if (!_hasActive) return false;
  out = _active; return true;
}
bool MessageStore::lastInbound(ConvoKey& out) const {
  if (!_hasLastInbound) return false;
  out = _lastInbound; return true;
}
int  MessageStore::repeatCount(const ConvoKey& key, int msgIndex) const {
  MsgRecord r; if (!getMessage(key, msgIndex, r)) return 0;
  if (r.kind != KIND_OUT_CHAN) return 0;
  for (int i = 0; i < MAX_TRACKED; i++)
    if (_tracked[i].used && _tracked[i].kind == KIND_OUT_CHAN &&
        _tracked[i].senderTime == r.senderTime && _tracked[i].key.equals(key))
      return _tracked[i].rptCount;
  return 0;   // aged out of tracking -> summary-only
}
bool MessageStore::getRepeat(const ConvoKey& key, int msgIndex, int rIdx, RepeatRec& out) const {
  MsgRecord r; if (!getMessage(key, msgIndex, r)) return false;
  for (int i = 0; i < MAX_TRACKED; i++)
    if (_tracked[i].used && _tracked[i].kind == KIND_OUT_CHAN &&
        _tracked[i].senderTime == r.senderTime && _tracked[i].key.equals(key)) {
      if (rIdx < 0 || rIdx >= _tracked[i].rptCount) return false;
      out = _tracked[i].repeats[rIdx]; return true;
    }
  return false;
}
void MessageStore::deleteMessage(const ConvoKey& key, int index) {
  int n = 0;
  for (uint32_t off = 0; off < _used; ) {
    uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (!recDead(r)) {
      ConvoKey k; rdKey(r, k);
      if (k.equals(key)) {
        if (n == index) {
          r[0] |= 0x04; _deadBytes += sz;
          int ci = findConvo(key);
          // Keep the convo summary even when its last message is deleted; a
          // chat only leaves the list via an explicit chat-delete (or eviction).
          if (ci >= 0) _convos[ci].count--;
          _seq++; return;
        }
        n++;
      }
    }
    off += sz;
  }
}

void MessageStore::clearConvo(const ConvoKey& key) {
  int ci = findConvo(key);
  if (ci < 0) return;
  // Tombstone every message but keep the convo summary so the chat stays in
  // the list (emptied, not deleted). Don't route through tombstoneOldestOf:
  // it drops the slot once count hits 0. Eviction paths still want that drop.
  for (uint32_t off = 0; off < _used; ) {
    uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (!recDead(r)) {
      ConvoKey k; rdKey(r, k);
      if (k.equals(key)) { r[0] |= 0x04; _deadBytes += sz; }
    }
    off += sz;
  }
  _convos[ci].count = 0;
  _convos[ci].unread = 0;
  _convos[ci].notifyUnread = 0;   // keep lastTime -> chat holds its place in the list
  _seq++;
}

void MessageStore::deleteConvo(const ConvoKey& key) {
  int ci = findConvo(key);
  if (ci < 0) return;
  clearConvo(key);          // tombstone every message (keeps the slot for now)
  dropConvoIfEmpty(ci);     // ...then drop the (now empty) summary entirely
  _seq++;
}

void MessageStore::markUnread(const ConvoKey& key) {
  int ci = findConvo(key);
  if (ci < 0 || _convos[ci].unread) return;   // already unread -> nothing to do
  _convos[ci].unread = 1;
  _seq++;
}

void MessageStore::markNotifiable(const ConvoKey& key) {
  int ci = findConvo(key);
  if (ci < 0) return;
  if (_hasActive && _active.equals(key)) return;   // open chat -> nothing to flag
  _convos[ci].notifyUnread++;
  _seq++;
}

uint16_t MessageStore::totalNotifyUnread() const {
  uint32_t t = 0;
  for (int i = 0; i < _convoCount; i++) t += _convos[i].notifyUnread;
  return t > 0xFFFF ? 0xFFFF : (uint16_t)t;
}

void MessageStore::ensureChannel(uint8_t channelIdx) {
  ConvoKey k = channelKey(channelIdx);
  if (findConvo(k) >= 0) return;   // already seeded or has messages
  ensureConvo(k);                  // empty slot; lastTime 0 -> sorts below active chats
  _seq++;                          // mark dirty: refresh the list + persist the chat
}
size_t MessageStore::serialize(uint8_t* out, size_t cap) const {
  const size_t CONVO_REC = 16;
  size_t need = 4 + 1 + 4 + 2 + (size_t)_convoCount * CONVO_REC + _used;
  if (cap < need) return 0;
  size_t i = 0;
  memcpy(out + i, "MMSG", 4); i += 4;
  out[i++] = 2;                                  // format version 2
  memcpy(out + i, &_used, 4); i += 4;
  uint16_t cc = (uint16_t)_convoCount; memcpy(out + i, &cc, 2); i += 2;
  for (int c = 0; c < _convoCount; c++) {
    out[i++] = _convos[c].key.type;
    memcpy(out + i, _convos[c].key.id, 6); i += 6;
    memcpy(out + i, &_convos[c].lastTime, 4); i += 4;
    memcpy(out + i, &_convos[c].unread, 2); i += 2;
    out[i++] = _convos[c].count;
    memcpy(out + i, &_convos[c].notifyUnread, 2); i += 2;
  }
  memcpy(out + i, _arena, _used); i += _used;
  return i;
}
bool MessageStore::deserialize(const uint8_t* in, size_t len) {
  if (len < 11 || memcmp(in, "MMSG", 4) != 0) return false;
  uint8_t ver = in[4];
  if (ver != 1 && ver != 2) return false;
  size_t i = 5;
  uint32_t used; memcpy(&used, in + i, 4); i += 4;
  uint16_t cc;   memcpy(&cc, in + i, 2);  i += 2;
  if (used > (uint32_t)ARENA_BYTES || cc > (uint16_t)MAX_CONVOS) return false;
  // v1 wrote whole padded ConvoSummary structs (16-byte stride); v2 writes
  // 16-byte packed records. Both strides are 16, disambiguated by `ver`.
  const size_t STRIDE = 16;
  if (len < i + (size_t)cc * STRIDE + used) return false;
  reset();
  _convoCount = (int)cc;
  for (int c = 0; c < (int)cc; c++) {
    const uint8_t* r = in + i + (size_t)c * STRIDE;
    ConvoSummary& s = _convos[c];
    if (ver == 2) {
      s.key.type = r[0];
      memcpy(s.key.id, r + 1, 6);
      memcpy(&s.lastTime, r + 7, 4);
      memcpy(&s.unread, r + 11, 2);
      s.count = r[13];
      memcpy(&s.notifyUnread, r + 14, 2);
    } else {                                  // ver == 1 (old padded layout)
      s.key.type = r[0];
      memcpy(s.key.id, r + 1, 6);
      memcpy(&s.lastTime, r + 8, 4);          // padded to a 4-byte boundary
      memcpy(&s.unread, r + 12, 2);
      s.count = r[14];
      s.notifyUnread = 0;
    }
  }
  i += (size_t)cc * STRIDE;
  memcpy(_arena, in + i, used); _used = used;
  _deadBytes = 0;
  for (uint32_t off = 0; off < _used; ) {
    const uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (recDead(r)) _deadBytes += sz;
    off += sz;
  }
  return true;
}
void MessageStore::appendOutboundDM(const ConvoKey& key, const char* text, uint16_t textLen,
                                    uint32_t senderTime, uint32_t sendTime,
                                    uint32_t expectedAck, uint32_t sentMillis) {
  if (textLen > MAX_TEXT) textLen = MAX_TEXT;
  if (!reserve(REC_HDR + textLen + 3)) return;
  int ci = ensureConvo(key); if (ci < 0) return;
  writeHeader(_arena, _used, KIND_OUT_DM, key.type == 1, key, text, textLen, senderTime, sendTime);
  _arena[_used++] = ST_PENDING;        // status
  _arena[_used++] = 0; _arena[_used++] = 0;  // tripTimeMs = 0
  _convos[ci].count++;
  if (_convos[ci].count > PER_CHAT_CAP) tombstoneOldestOf(key);
  touchConvo(ci, sendTime);
  Tracked* t = openTracked(key, senderTime, KIND_OUT_DM);
  t->expectedAck = expectedAck; (void)sentMillis;
  _seq++;
}
void MessageStore::appendOutboundChannel(const ConvoKey& key, const char* text, uint16_t textLen,
                                         uint32_t senderTime, uint32_t sendTime) {
  if (textLen > MAX_TEXT) textLen = MAX_TEXT;
  if (!reserve(REC_HDR + textLen + 2)) return;
  int ci = ensureConvo(key); if (ci < 0) return;
  writeHeader(_arena, _used, KIND_OUT_CHAN, true, key, text, textLen, senderTime, sendTime);
  _arena[_used++] = ST_PENDING;   // status
  _arena[_used++] = 0;            // heardCount
  _convos[ci].count++;
  if (_convos[ci].count > PER_CHAT_CAP) tombstoneOldestOf(key);
  touchConvo(ci, sendTime);
  openTracked(key, senderTime, KIND_OUT_CHAN);
  _seq++;
}
void MessageStore::markDelivered(uint32_t expectedAck, uint16_t tripTimeMs) {
  for (int i = 0; i < MAX_TRACKED; i++) {
    if (_tracked[i].used && _tracked[i].kind == KIND_OUT_DM && _tracked[i].expectedAck == expectedAck) {
      for (uint32_t off = 0; off < _used; ) {
        uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
        if (!recDead(r) && recKind(r) == KIND_OUT_DM) {
          ConvoKey k; rdKey(r, k);
          if (k.equals(_tracked[i].key) && rd32(r + 8) == _tracked[i].senderTime) {
            uint8_t* trailer = r + REC_HDR + recTextLen(r);
            trailer[0] = ST_DELIVERED; memcpy(trailer + 1, &tripTimeMs, 2);
            _seq++; return;
          }
        }
        off += sz;
      }
      return;
    }
  }
}
void MessageStore::markFailed(const ConvoKey& key, uint32_t senderTime) {
  for (uint32_t off = 0; off < _used; ) {
    uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (!recDead(r) && recKind(r) == KIND_OUT_DM) {
      ConvoKey k; rdKey(r, k);
      if (k.equals(key) && rd32(r + 8) == senderTime) {
        uint8_t* trailer = r + REC_HDR + recTextLen(r);
        if (trailer[0] == ST_PENDING) { trailer[0] = ST_FAILED; _seq++; }
        return;
      }
    }
    off += sz;
  }
}

void MessageStore::updateExpectedAck(const ConvoKey& key, uint32_t senderTime, uint32_t newAck) {
  Tracked* t = findTracked(key, senderTime);
  if (!t) t = openTracked(key, senderTime, KIND_OUT_DM);
  if (t) t->expectedAck = newAck;
}

int MessageStore::pendingDMCount() const {
  int n = 0;
  for (uint32_t off = 0; off < _used; ) {
    const uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (!recDead(r) && recKind(r) == KIND_OUT_DM) {
      const uint8_t* trailer = r + REC_HDR + recTextLen(r);
      if (trailer[0] == ST_PENDING) n++;
    }
    off += sz;
  }
  return n;
}

bool MessageStore::getPendingDM(int index, ConvoKey& outKey, uint32_t& outSenderTime) const {
  int n = 0;
  for (uint32_t off = 0; off < _used; ) {
    const uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (!recDead(r) && recKind(r) == KIND_OUT_DM) {
      const uint8_t* trailer = r + REC_HDR + recTextLen(r);
      if (trailer[0] == ST_PENDING) {
        if (n == index) { rdKey(r, outKey); outSenderTime = rd32(r + 8); return true; }
        n++;
      }
    }
    off += sz;
  }
  return false;
}

int MessageStore::getDMText(const ConvoKey& key, uint32_t senderTime, char* buf, int cap) const {
  if (!buf || cap <= 0) return 0;
  buf[0] = 0;
  for (uint32_t off = 0; off < _used; ) {
    const uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (!recDead(r) && recKind(r) == KIND_OUT_DM) {
      ConvoKey k; rdKey(r, k);
      if (k.equals(key) && rd32(r + 8) == senderTime) {
        uint16_t len = recTextLen(r);
        if (len > (uint16_t)(cap - 1)) len = (uint16_t)(cap - 1);
        memcpy(buf, r + REC_HDR, len); buf[len] = 0;
        return len;
      }
    }
    off += sz;
  }
  return 0;
}

void MessageStore::addRepeat(const ConvoKey& key, uint32_t senderTime,
                             int8_t snrx4, const uint8_t* path, uint8_t pathLen) {
  Tracked* t = findTracked(key, senderTime);
  if (!t || t->kind != KIND_OUT_CHAN) return;
  if (pathLen > MAX_PATH) pathLen = MAX_PATH;
  if (t->rptCount < MAX_REPEATS) {
    RepeatRec& rr = t->repeats[t->rptCount];
    rr.snrx4 = snrx4; rr.pathLen = pathLen; rr.hops = pathLen;
    memcpy(t->rptStore[t->rptCount], path, pathLen);
    rr.path = t->rptStore[t->rptCount];
    t->rptCount++;
  }
  // bump heardCount + status in the arena record (heardCount saturates at 255)
  for (uint32_t off = 0; off < _used; ) {
    uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (!recDead(r) && recKind(r) == KIND_OUT_CHAN) {
      ConvoKey k; rdKey(r, k);
      if (k.equals(key) && rd32(r + 8) == senderTime) {
        uint8_t* trailer = r + REC_HDR + recTextLen(r);
        trailer[0] = ST_SENT;
        if (trailer[1] < 255) trailer[1]++;
        _seq++; return;
      }
    }
    off += sz;
  }
}
void MessageStore::compact() {
  if (_deadBytes == 0) return;
  uint32_t w = 0;
  for (uint32_t off = 0; off < _used; ) {
    const uint8_t* r = _arena + off;
    uint32_t sz = recordSize(r);
    if (!recDead(r)) { if (w != off) memmove(_arena + w, r, sz); w += sz; }
    off += sz;
  }
  _used = w; _deadBytes = 0;
}

bool MessageStore::reserve(uint32_t need) {
  if (need > (uint32_t)ARENA_BYTES) return false;
  if (_used + need <= (uint32_t)ARENA_BYTES) return true;
  compact();
  int guard = MAX_CONVOS * PER_CHAT_CAP;
  while (_used + need > (uint32_t)ARENA_BYTES && guard-- > 0) {
    evictOldestOfLargest();
    compact();
  }
  return _used + need <= (uint32_t)ARENA_BYTES;
}

void MessageStore::evictOldestOfLargest() {
  // find convo with the most messages (ties -> oldest lastTime)
  int big = -1; uint8_t bestCount = 0; uint32_t bestTime = 0xFFFFFFFF;
  for (int i = 0; i < _convoCount; i++) {
    if (_convos[i].count > bestCount ||
        (_convos[i].count == bestCount && _convos[i].lastTime < bestTime)) {
      big = i; bestCount = _convos[i].count; bestTime = _convos[i].lastTime;
    }
  }
  if (big < 0) return;
  tombstoneOldestOf(_convos[big].key);   // may drop the convo summary if count hits 0
}

void MessageStore::dropConvoIfEmpty(int ci) {
  if (_convos[ci].count == 0) {
    for (int i = ci; i < _convoCount - 1; i++) _convos[i] = _convos[i + 1];
    _convoCount--;
  }
}

void MessageStore::tombstoneOldestOf(const ConvoKey& key) {
  for (uint32_t off = 0; off < _used; ) {
    uint8_t* r = _arena + off; uint32_t sz = recordSize(r);
    if (!recDead(r)) {
      ConvoKey k; rdKey(r, k);
      if (k.equals(key)) {
        r[0] |= 0x04; _deadBytes += sz;
        int ci = findConvo(key);
        if (ci >= 0) { _convos[ci].count--; dropConvoIfEmpty(ci); }
        return;
      }
    }
    off += sz;
  }
}
MessageStore::Tracked* MessageStore::openTracked(const ConvoKey& key, uint32_t senderTime, uint8_t kind) {
  int slot = -1;
  for (int i = 0; i < MAX_TRACKED; i++) if (!_tracked[i].used) { slot = i; break; }
  if (slot < 0) {
    memmove(&_tracked[0], &_tracked[1], sizeof(Tracked) * (MAX_TRACKED - 1));
    // re-anchor each survivor's repeat paths to their (now-shifted) backing store
    for (int i = 0; i < MAX_TRACKED - 1; i++) {
      if (!_tracked[i].used) continue;
      for (int j = 0; j < _tracked[i].rptCount; j++)
        _tracked[i].repeats[j].path = _tracked[i].rptStore[j];
    }
    slot = MAX_TRACKED - 1;
  }
  Tracked& t = _tracked[slot];
  memset(&t, 0, sizeof(Tracked));
  t.used = true; t.key = key; t.senderTime = senderTime; t.kind = kind;
  return &t;
}
MessageStore::Tracked* MessageStore::findTracked(const ConvoKey& key, uint32_t senderTime) {
  for (int i = 0; i < MAX_TRACKED; i++)
    if (_tracked[i].used && _tracked[i].senderTime == senderTime && _tracked[i].key.equals(key))
      return &_tracked[i];
  return nullptr;
}

} // namespace mishmesh
