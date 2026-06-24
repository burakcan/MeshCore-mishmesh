// mishmesh/core/MessageStore.cpp
#include "MessageStore.h"
#include "MsgCodec.h"
namespace mishmesh {

ConvoKey directKey(const uint8_t* p)   { ConvoKey k; k.type = 0; memcpy(k.id, p, 6); return k; }
ConvoKey channelKey(uint8_t idx)       { ConvoKey k; k.type = 1; memset(k.id, 0, 6); k.id[0] = idx; return k; }

void MessageStore::reset() {
  _backend = nullptr;
  _index.reset();
  _seq = 0;
  _hasActive = false;
  _hasLastInbound = false;
  memset(_tracked, 0, sizeof(_tracked));
  _windowValid = false;
  _winCount = _winFirst = _winTotal = 0;
}

void MessageStore::begin(MsgLogBackend* b) {
  _backend = b;
  _index.reset();
  _seq = 0;
  _hasActive = _hasLastInbound = false;
  memset(_tracked, 0, sizeof(_tracked));
  _windowValid = false;
  _winCount = _winFirst = _winTotal = 0;
  if (b) {
    uint32_t n = b->loadIndex(_idxBuf, sizeof(_idxBuf));
    if (!n || !_index.deserialize(_idxBuf, n)) rebuildIndex();
  }
}

// ---- window management ----

void MessageStore::loadWindow(const ConvoKey& key) const {
  _windowKey   = key;
  _windowValid = false;
  _winCount    = 0;
  _winFirst    = 0;
  _winTotal    = 0;

  if (!_backend) { _windowValid = true; return; }
  char name[15]; codec::keyToName(key, name);
  uint32_t fileSize = _backend->size(name);
  if (fileSize == 0) { _windowValid = true; return; }

  // Pass 1: scan the whole file counting live records and recording their offsets
  // so we know which tail records fit in WINDOW_BYTES.
  // We work in _recBuf-sized chunks to find record boundaries without a big buffer.
  // offsets[] stores the byte offset of each live record (we only need the tail ones).
  // We keep a rolling ring of the last PER_CHAT_CAP offsets.
  static const int MAX_LIVE = PER_CHAT_CAP; // can't exceed cap after rotation
  uint32_t liveOff[MAX_LIVE];   // byte offsets of last MAX_LIVE live records
  uint32_t liveSz[MAX_LIVE];    // sizes
  int liveHead = 0, liveUsed = 0;
  int totalLive = 0;

  uint32_t pos = 0;
  while (pos < fileSize) {
    // Read up to MAX_REC bytes so recSize() can dereference the full trailer
    // (KIND_INBOUND reads pathLen at REC_HDR+textLen+1, beyond REC_HDR bytes).
    uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
    if (got < (uint32_t)codec::REC_HDR) break;
    uint32_t sz = (uint32_t)codec::recSize(_recBuf);
    if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
    if (pos + sz > fileSize) break;
    if (!codec::recDead(_recBuf)) {
      liveOff[liveHead] = pos;
      liveSz[liveHead]  = sz;
      liveHead = (liveHead + 1) % MAX_LIVE;
      if (liveUsed < MAX_LIVE) liveUsed++;
      totalLive++;
    }
    pos += sz;
  }
  _winTotal = totalLive;

  // Find how many tail records fit in WINDOW_BYTES (scan backwards through liveOff ring)
  uint32_t arenaUsed = 0;
  int fitCount = 0;
  for (int i = 0; i < liveUsed; i++) {
    // Walk ring backwards: index of i-th-from-end
    int ri = (liveHead - 1 - i + MAX_LIVE) % MAX_LIVE;
    if (arenaUsed + liveSz[ri] > (uint32_t)WINDOW_BYTES) break;
    arenaUsed += liveSz[ri];
    fitCount++;
  }

  _winCount = fitCount;
  _winFirst = totalLive - fitCount;

  // Fill _arena: read those tail records in forward order
  uint32_t arenaPos = 0;
  for (int i = fitCount - 1; i >= 0; i--) {
    int ri = (liveHead - 1 - i + MAX_LIVE) % MAX_LIVE;
    uint32_t off = liveOff[ri];
    uint32_t sz  = liveSz[ri];
    _winOffs[fitCount - 1 - i] = (uint16_t)arenaPos;
    _backend->read(name, off, _arena + arenaPos, sz);
    arenaPos += sz;
  }

  _windowValid = true;
}

void MessageStore::invalidateWindow(const ConvoKey& key) {
  if (_windowValid && _windowKey.equals(key)) _windowValid = false;
}

// ---- streaming helper ----

bool MessageStore::findRecordOffset(const char* name, int targetIdx,
                                    uint32_t& offOut, uint32_t& lenOut) const {
  if (!_backend) return false;
  uint32_t fileSize = _backend->size(name);
  uint32_t pos = 0;
  int n = 0;
  while (pos < fileSize) {
    uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
    if (got < (uint32_t)codec::REC_HDR) break;
    uint32_t sz = (uint32_t)codec::recSize(_recBuf);
    if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
    if (pos + sz > fileSize) break;
    if (!codec::recDead(_recBuf)) {
      if (n == targetIdx) { offOut = pos; lenOut = sz; return true; }
      n++;
    }
    pos += sz;
  }
  return false;
}

// ---- index rebuild ----

void MessageStore::rebuildIndex() {
  _index.reset();
  if (!_backend) return;
  MsgLogInfo infos[MAX_CONVOS];
  int m = _backend->list(infos, MAX_CONVOS);
  for (int i = 0; i < m; i++) {
    ConvoKey k;
    // Bad filename -> stray file; remove it and move on.
    if (!codec::nameToKey(infos[i].name, k)) { _backend->remove(infos[i].name); continue; }
    uint32_t fileSize = _backend->size(infos[i].name);
    int liveCount = 0;
    uint32_t maxT = 0;
    uint32_t pos = 0;
    uint32_t validEnd = 0;  // byte offset past the last fully-verified record
    // Track last live record's text for preview; slot doesn't exist until after the loop.
    char previewBuf[PREVIEW_LEN];
    uint16_t previewLen = 0;
    while (pos < fileSize) {
      uint32_t got = _backend->read(infos[i].name, pos, _recBuf, (uint32_t)codec::MAX_REC);
      if (got < (uint32_t)codec::REC_HDR) break;
      uint32_t sz = (uint32_t)codec::recSize(_recBuf);
      if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
      if (pos + sz > fileSize) break;  // torn tail: record overruns the file
      if (!codec::recDead(_recBuf)) {
        liveCount++;
        uint32_t lt = codec::rd32(_recBuf + 12);
        uint32_t st = codec::rd32(_recBuf + 8);
        uint32_t t  = lt ? lt : st;
        if (t > maxT) maxT = t;
        uint16_t tlen = codec::recTextLen(_recBuf);
        if (tlen) {
          uint16_t cap = (uint16_t)(PREVIEW_LEN - 1);
          previewLen = tlen < cap ? tlen : cap;
          memcpy(previewBuf, _recBuf + codec::REC_HDR, previewLen);
        }
      }
      pos += sz;
      validEnd = pos;
    }
    // No live records -> nothing to index; remove the dead/empty file.
    // [note] Cleared chats (0-byte files) are also removed here and not preserved in the
    // conversation list after a rebuild - this is intentional; do not change this behavior.
    if (liveCount == 0) { _backend->remove(infos[i].name); continue; }
    // Heal a torn tail: truncate to the last complete record boundary.
    // truncate() is buffer-free (no size limit), so this works for any file size.
    if (validEnd < fileSize) _backend->truncate(infos[i].name, validEnd);
    ConvoSummary& c = ensureSlot(k);
    c.count    = (uint16_t)liveCount;
    c.lastTime = maxT;
    c.logBytes = validEnd;  // valid-record bytes only (torn tail excluded)
    if (previewLen) _index.setPreview(k, previewBuf, previewLen);
  }
}

void MessageStore::saveIndex() {
  if (!_backend) return;
  uint32_t n = _index.serialize(_idxBuf, sizeof(_idxBuf));
  if (n) _backend->saveIndex(_idxBuf, n);
}

// ---- internal helpers ----

bool MessageStore::ensureSpace(const ConvoKey& key, uint32_t need) {
  if (!_backend) return true;
  while (_index.totalLogBytes() + need > MSG_FLASH_BUDGET || _backend->freeBytes() < need) {
    if (_index.count() == 0) break;
    if (_index.lruKey().equals(key)) return false;  // can't evict the append target; drop message
    ConvoKey v; _index.evictLRU(v);                  // guaranteed != key
    char vn[15]; codec::keyToName(v, vn);
    _backend->remove(vn);
    if (_windowValid && _windowKey.equals(v)) _windowValid = false;
  }
  return true;
}

ConvoSummary& MessageStore::ensureSlot(const ConvoKey& key) {
  if (_index.count() >= MAX_CONVOS && _index.find(key) < 0) {
    ConvoKey victim;
    if (_index.evictLRU(victim) && _backend) {
      char name[15]; codec::keyToName(victim, name);
      _backend->remove(name);
    }
  }
  return _index.ensure(key);
}

void MessageStore::rotateIfNeeded(const ConvoKey& key, ConvoSummary& c) {
  if (!_backend || c.count < PER_CHAT_CAP) return;
  char name[15]; codec::keyToName(key, name);
  // Batch rotation: drop ROTATE_DROP live records in one dropFront, amortising
  // the cost of a full tail-rewrite over multiple appends (vs. one per message).
  static const int ROTATE_DROP = 16;
  uint32_t fileSize = _backend->size(name);
  uint32_t cut = 0;
  uint32_t pos = 0;
  int live = 0;
  while (pos < fileSize) {
    uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
    if (got < (uint32_t)codec::REC_HDR) break;
    uint32_t sz = (uint32_t)codec::recSize(_recBuf);
    if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
    if (pos + sz > fileSize) break;
    if (!codec::recDead(_recBuf)) {
      live++;
      if (live == ROTATE_DROP) { cut = pos + sz; break; }
    }
    pos += sz;
  }
  if (cut == 0 || cut > fileSize) return;
  _backend->dropFront(name, cut);
  c.count = (c.count >= (uint16_t)live) ? c.count - (uint16_t)live : 0;
  c.logBytes = (c.logBytes >= cut) ? c.logBytes - cut : 0;
  invalidateWindow(key);
}

// ---- capture ----

void MessageStore::appendInbound(const ConvoKey& key, const char* text, uint16_t textLen,
                                  uint32_t senderTime, uint32_t recvTime,
                                  int8_t snrx4, const uint8_t* path, uint8_t pathLen) {
  if (textLen > MAX_TEXT) textLen = MAX_TEXT;
  if (pathLen > MAX_PATH) pathLen = MAX_PATH;

  if (_backend) {
    uint32_t recLen = (uint32_t)(codec::REC_HDR + textLen + 2 + pathLen);
    if (!ensureSpace(key, recLen)) return;
  }

  ConvoSummary& c = ensureSlot(key);
  if (_backend) rotateIfNeeded(key, c);

  if (_backend) {
    uint8_t trailer[2 + MAX_PATH];
    trailer[0] = (uint8_t)snrx4;
    trailer[1] = pathLen;
    if (pathLen) memcpy(trailer + 2, path, pathLen);
    int n = codec::writeRecord(_recBuf, KIND_INBOUND, key, text, textLen,
                               senderTime, recvTime, trailer, (uint8_t)(2 + pathLen));
    char name[15]; codec::keyToName(key, name);
    if (!_backend->append(name, _recBuf, (uint32_t)n)) return;
    c.logBytes += (uint32_t)n;
    invalidateWindow(key);
  }

  c.count++;
  if (!(_hasActive && _active.equals(key))) c.unread++;
  int ci = _index.find(key);
  _index.touch(ci, recvTime ? recvTime : senderTime);
  _index.setPreview(key, text, textLen);
  _lastInbound = key; _hasLastInbound = true;
  _seq++;
}

void MessageStore::appendOutboundDM(const ConvoKey& key, const char* text, uint16_t textLen,
                                     uint32_t senderTime, uint32_t sendTime,
                                     uint32_t expectedAck, uint32_t sentMillis) {
  if (textLen > MAX_TEXT) textLen = MAX_TEXT;

  if (_backend) {
    uint32_t recLen = (uint32_t)(codec::REC_HDR + textLen + 3);
    if (!ensureSpace(key, recLen)) return;
  }

  ConvoSummary& c = ensureSlot(key);
  if (_backend) rotateIfNeeded(key, c);

  if (_backend) {
    uint8_t trailer[3] = { ST_PENDING, 0, 0 };
    int n = codec::writeRecord(_recBuf, KIND_OUT_DM, key, text, textLen,
                               senderTime, sendTime, trailer, 3);
    char name[15]; codec::keyToName(key, name);
    if (!_backend->append(name, _recBuf, (uint32_t)n)) return;
    c.logBytes += (uint32_t)n;
    invalidateWindow(key);
  }

  c.count++;
  int ci = _index.find(key);
  _index.touch(ci, sendTime);
  _index.setPreview(key, text, textLen);
  Tracked* t = openTracked(key, senderTime, KIND_OUT_DM);
  t->expectedAck = expectedAck; (void)sentMillis;
  _seq++;
}

void MessageStore::appendOutboundChannel(const ConvoKey& key, const char* text, uint16_t textLen,
                                          uint32_t senderTime, uint32_t sendTime) {
  if (textLen > MAX_TEXT) textLen = MAX_TEXT;

  if (_backend) {
    uint32_t recLen = (uint32_t)(codec::REC_HDR + textLen + 2);
    if (!ensureSpace(key, recLen)) return;
  }

  ConvoSummary& c = ensureSlot(key);
  if (_backend) rotateIfNeeded(key, c);

  if (_backend) {
    uint8_t trailer[2] = { ST_PENDING, 0 };
    int n = codec::writeRecord(_recBuf, KIND_OUT_CHAN, key, text, textLen,
                               senderTime, sendTime, trailer, 2);
    char name[15]; codec::keyToName(key, name);
    if (!_backend->append(name, _recBuf, (uint32_t)n)) return;
    c.logBytes += (uint32_t)n;
    invalidateWindow(key);
  }

  c.count++;
  int ci = _index.find(key);
  _index.touch(ci, sendTime);
  _index.setPreview(key, text, textLen);
  openTracked(key, senderTime, KIND_OUT_CHAN);
  _seq++;
}

void MessageStore::markDelivered(uint32_t expectedAck, uint16_t tripTimeMs) {
  for (int i = 0; i < MAX_TRACKED; i++) {
    if (!_tracked[i].used || _tracked[i].kind != KIND_OUT_DM) continue;
    if (_tracked[i].expectedAck != expectedAck) continue;
    if (!_backend) { _seq++; return; }
    char name[15]; codec::keyToName(_tracked[i].key, name);
    // Find the matching record's byte offset using senderTime
    uint32_t fileSize = _backend->size(name);
    uint32_t pos = 0;
    while (pos < fileSize) {
      uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
      if (got < (uint32_t)codec::REC_HDR) break;
      uint32_t sz = (uint32_t)codec::recSize(_recBuf);
      if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
      if (pos + sz > fileSize) break;
      if (!codec::recDead(_recBuf) && codec::recKind(_recBuf) == KIND_OUT_DM) {
        ConvoKey k; codec::rdKey(_recBuf, k);
        if (k.equals(_tracked[i].key) && codec::rd32(_recBuf + 8) == _tracked[i].senderTime) {
          uint16_t textLen = codec::recTextLen(_recBuf);
          uint32_t trailerOff = pos + (uint32_t)codec::REC_HDR + textLen;
          uint8_t patch[3] = { ST_DELIVERED, (uint8_t)(tripTimeMs & 0xFF), (uint8_t)(tripTimeMs >> 8) };
          _backend->patch(name, trailerOff, patch, 3);
          invalidateWindow(_tracked[i].key);
          _seq++; return;
        }
      }
      pos += sz;
    }
    return;
  }
}

void MessageStore::markFailed(const ConvoKey& key, uint32_t senderTime) {
  if (!_backend) return;
  char name[15]; codec::keyToName(key, name);
  uint32_t fileSize = _backend->size(name);
  uint32_t pos = 0;
  while (pos < fileSize) {
    uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
    if (got < (uint32_t)codec::REC_HDR) break;
    uint32_t sz = (uint32_t)codec::recSize(_recBuf);
    if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
    if (pos + sz > fileSize) break;
    if (!codec::recDead(_recBuf) && codec::recKind(_recBuf) == KIND_OUT_DM) {
      ConvoKey k; codec::rdKey(_recBuf, k);
      if (k.equals(key) && codec::rd32(_recBuf + 8) == senderTime) {
        uint16_t textLen = codec::recTextLen(_recBuf);
        const uint8_t* trailerInBuf = _recBuf + codec::REC_HDR + textLen;
        if (trailerInBuf[0] == ST_PENDING) {
          uint32_t trailerOff = pos + (uint32_t)codec::REC_HDR + textLen;
          uint8_t patch[1] = { ST_FAILED };
          _backend->patch(name, trailerOff, patch, 1);
          invalidateWindow(key);
          _seq++;
        }
        return;
      }
    }
    pos += sz;
  }
}

void MessageStore::updateExpectedAck(const ConvoKey& key, uint32_t senderTime, uint32_t newAck) {
  Tracked* t = findTracked(key, senderTime);
  if (!t) t = openTracked(key, senderTime, KIND_OUT_DM);
  if (t) t->expectedAck = newAck;
}

void MessageStore::addRepeat(const ConvoKey& key, uint32_t senderTime,
                              int8_t snrx4, const uint8_t* path, uint8_t pathLen) {
  Tracked* t = findTracked(key, senderTime);
  if (!t || t->kind != KIND_OUT_CHAN) return;
  if (pathLen > MAX_PATH) pathLen = MAX_PATH;
  if (t->rptCount < MAX_REPEATS) {
    RepeatRec& rr = t->repeats[t->rptCount];
    rr.snrx4 = snrx4; rr.pathLen = pathLen; rr.hops = pathLen;
    if (pathLen && path) memcpy(t->rptStore[t->rptCount], path, pathLen);
    rr.path = t->rptStore[t->rptCount];
    t->rptCount++;
  }
  if (!_backend) { _seq++; return; }
  char name[15]; codec::keyToName(key, name);
  uint32_t fileSize = _backend->size(name);
  uint32_t pos = 0;
  while (pos < fileSize) {
    uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
    if (got < (uint32_t)codec::REC_HDR) break;
    uint32_t sz = (uint32_t)codec::recSize(_recBuf);
    if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
    if (pos + sz > fileSize) break;
    if (!codec::recDead(_recBuf) && codec::recKind(_recBuf) == KIND_OUT_CHAN) {
      ConvoKey k; codec::rdKey(_recBuf, k);
      if (k.equals(key) && codec::rd32(_recBuf + 8) == senderTime) {
        uint16_t textLen = codec::recTextLen(_recBuf);
        uint32_t trailerOff = pos + (uint32_t)codec::REC_HDR + textLen;
        const uint8_t* trailerInBuf = _recBuf + codec::REC_HDR + textLen;
        uint8_t heard = trailerInBuf[1] < 255 ? trailerInBuf[1] + 1 : 255;
        uint8_t patch[2] = { ST_SENT, heard };
        _backend->patch(name, trailerOff, patch, 2);
        invalidateWindow(key);
        _seq++; return;
      }
    }
    pos += sz;
  }
  _seq++;
}

// ---- queries ----

int MessageStore::messageCount(const ConvoKey& key) const {
  int ci = _index.find(key);
  return ci < 0 ? 0 : (int)_index.rawCount(ci);
}

void MessageStore::decodeRecord(const uint8_t* r, const ConvoKey& key, MsgRecord& out) const {
  out.key  = key; out.kind = codec::recKind(r);
  out.senderTime = codec::rd32(r + 8); out.localTime = codec::rd32(r + 12);
  out.textLen = codec::recTextLen(r); out.text = (const char*)(r + codec::REC_HDR);
  out.snrx4 = 0; out.hops = 0; out.path = nullptr; out.pathLen = 0;
  out.status = ST_PENDING; out.tripTimeMs = 0; out.heardCount = 0;
  const uint8_t* t = r + codec::REC_HDR + out.textLen;
  if (out.kind == KIND_INBOUND) {
    out.snrx4 = (int8_t)t[0]; out.pathLen = t[1]; out.hops = t[1];
    out.path = t + 2;
  } else if (out.kind == KIND_OUT_DM) {
    out.status = t[0]; memcpy(&out.tripTimeMs, t + 1, 2);
  } else {
    out.status = t[0]; out.heardCount = t[1];
    // heardCount is maintained in flash by addRepeat - no _tracked overlay needed
  }
}

void MessageStore::forEachMessage(const ConvoKey& key,
                                  void (*cb)(void*, int, const MsgRecord&), void* ctx) const {
  if (!_backend || !cb) return;
  char name[15]; codec::keyToName(key, name);
  uint32_t fileSize = _backend->size(name);
  uint32_t pos = 0;
  int idx = 0;
  MsgRecord out;
  while (pos < fileSize) {
    uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
    if (got < (uint32_t)codec::REC_HDR) break;
    uint32_t sz = (uint32_t)codec::recSize(_recBuf);
    if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
    if (pos + sz > fileSize) break;                 // torn tail
    if (!codec::recDead(_recBuf)) {
      ConvoKey k; codec::rdKey(_recBuf, k);
      if (k.equals(key)) {
        decodeRecord(_recBuf, key, out);            // out.text points into _recBuf
        cb(ctx, idx, out);                          // cb must consume before we read again
        idx++;
      }
    }
    pos += sz;
  }
}

bool MessageStore::getMessage(const ConvoKey& key, int index, MsgRecord& out) const {
  if (!_backend) return false;

  // Ensure window is loaded for this key
  if (!_windowValid || !_windowKey.equals(key)) loadWindow(key);

  if (index >= _winFirst) {
    // In window: zero-copy from _arena
    int wi = index - _winFirst;
    if (wi < _winCount) {
      decodeRecord(_arena + _winOffs[wi], key, out);
      return true;
    }
  }

  // Page: stream to the index-th live record
  char name[15]; codec::keyToName(key, name);
  uint32_t fileSize = _backend->size(name);
  uint32_t pos = 0;
  int n = 0;
  while (pos < fileSize) {
    uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
    if (got < (uint32_t)codec::REC_HDR) break;
    uint32_t sz = (uint32_t)codec::recSize(_recBuf);
    if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
    if (pos + sz > fileSize) break;
    if (!codec::recDead(_recBuf)) {
      ConvoKey k; codec::rdKey(_recBuf, k);
      if (k.equals(key)) {
        if (n == index) {
          decodeRecord(_recBuf, key, out);
          return true;
        }
        n++;
      }
    }
    pos += sz;
  }
  return false;
}

int  MessageStore::convoCount() const { return _index.count(); }

bool MessageStore::getConvo(int index, ConvoSummary& out) const {
  return _index.get(index, out);
}

uint16_t MessageStore::totalUnread() const       { return _index.totalUnread(); }
uint16_t MessageStore::totalNotifyUnread() const { return _index.totalNotifyUnread(); }

void MessageStore::setActiveConvo(const ConvoKey& key) {
  _active = key; _hasActive = true;
  int ci = _index.find(key);
  if (ci >= 0) {
    ConvoSummary& c = _index.ensure(key);
    if (c.unread || c.notifyUnread) { c.unread = 0; c.notifyUnread = 0; _seq++; }
  }
}
void MessageStore::clearActiveConvo() { _hasActive = false; }
bool MessageStore::activeConvo(ConvoKey& out) const {
  if (!_hasActive) return false; out = _active; return true;
}
bool MessageStore::lastInbound(ConvoKey& out) const {
  if (!_hasLastInbound) return false; out = _lastInbound; return true;
}

int MessageStore::repeatCount(const ConvoKey& key, int msgIndex) const {
  MsgRecord r; if (!getMessage(key, msgIndex, r)) return 0;
  if (r.kind != KIND_OUT_CHAN) return 0;
  for (int i = 0; i < MAX_TRACKED; i++)
    if (_tracked[i].used && _tracked[i].kind == KIND_OUT_CHAN &&
        _tracked[i].senderTime == r.senderTime && _tracked[i].key.equals(key))
      return _tracked[i].rptCount;
  return 0;
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

int MessageStore::pendingDMCount() const {
  if (!_backend) return 0;
  int total = 0;
  int n = _index.count();
  for (int ci = 0; ci < n; ci++) {
    ConvoSummary cs; if (!_index.get(ci, cs)) continue;
    char name[15]; codec::keyToName(cs.key, name);
    uint32_t fileSize = _backend->size(name);
    uint32_t pos = 0;
    while (pos < fileSize) {
      uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
      if (got < (uint32_t)codec::REC_HDR) break;
      uint32_t sz = (uint32_t)codec::recSize(_recBuf);
      if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
      if (pos + sz > fileSize) break;
      if (!codec::recDead(_recBuf) && codec::recKind(_recBuf) == KIND_OUT_DM) {
        const uint8_t* trailer = _recBuf + codec::REC_HDR + codec::recTextLen(_recBuf);
        if (trailer[0] == ST_PENDING) total++;
      }
      pos += sz;
    }
  }
  return total;
}

bool MessageStore::getPendingDM(int index, ConvoKey& outKey, uint32_t& outSenderTime) const {
  if (!_backend) return false;
  int n = 0, count = _index.count();
  for (int ci = 0; ci < count; ci++) {
    ConvoSummary cs; if (!_index.get(ci, cs)) continue;
    char name[15]; codec::keyToName(cs.key, name);
    uint32_t fileSize = _backend->size(name);
    uint32_t pos = 0;
    while (pos < fileSize) {
      uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
      if (got < (uint32_t)codec::REC_HDR) break;
      uint32_t sz = (uint32_t)codec::recSize(_recBuf);
      if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
      if (pos + sz > fileSize) break;
      if (!codec::recDead(_recBuf) && codec::recKind(_recBuf) == KIND_OUT_DM) {
        const uint8_t* trailer = _recBuf + codec::REC_HDR + codec::recTextLen(_recBuf);
        if (trailer[0] == ST_PENDING) {
          if (n == index) {
            codec::rdKey(_recBuf, outKey); outSenderTime = codec::rd32(_recBuf + 8); return true;
          }
          n++;
        }
      }
      pos += sz;
    }
  }
  return false;
}

int MessageStore::getDMText(const ConvoKey& key, uint32_t senderTime, char* buf, int cap) const {
  if (!buf || cap <= 0 || !_backend) return 0;
  buf[0] = 0;
  char name[15]; codec::keyToName(key, name);
  uint32_t fileSize = _backend->size(name);
  uint32_t pos = 0;
  while (pos < fileSize) {
    uint32_t got = _backend->read(name, pos, _recBuf, (uint32_t)codec::MAX_REC);
    if (got < (uint32_t)codec::REC_HDR) break;
    uint32_t sz = (uint32_t)codec::recSize(_recBuf);
    if (sz == 0 || sz > (uint32_t)codec::MAX_REC) break;
    if (pos + sz > fileSize) break;
    if (!codec::recDead(_recBuf) && codec::recKind(_recBuf) == KIND_OUT_DM) {
      ConvoKey k; codec::rdKey(_recBuf, k);
      if (k.equals(key) && codec::rd32(_recBuf + 8) == senderTime) {
        uint16_t len = codec::recTextLen(_recBuf);
        if (len > (uint16_t)(cap - 1)) len = (uint16_t)(cap - 1);
        memcpy(buf, _recBuf + codec::REC_HDR, len); buf[len] = 0;
        return len;
      }
    }
    pos += sz;
  }
  return 0;
}

void MessageStore::deleteMessage(const ConvoKey& key, int index) {
  int ci = _index.find(key);
  if (ci < 0) return;
  if (!_backend) {
    ConvoSummary& c = _index.ensure(key);
    if (c.count > 0) { c.count--; _seq++; }
    return;
  }
  char name[15]; codec::keyToName(key, name);
  uint32_t offOut = 0, lenOut = 0;
  if (!findRecordOffset(name, index, offOut, lenOut)) return;
  _backend->removeRange(name, offOut, lenOut);
  ConvoSummary& c = _index.ensure(key);
  if (c.count > 0) c.count--;
  c.logBytes = (c.logBytes >= lenOut) ? c.logBytes - lenOut : 0;
  invalidateWindow(key);
  _seq++;
}

void MessageStore::clearConvo(const ConvoKey& key) {
  int ci = _index.find(key);
  if (ci < 0) return;
  if (_backend) {
    char name[15]; codec::keyToName(key, name);
    _backend->rewrite(name, _recBuf, 0);   // empty file; _recBuf is valid pointer with len=0
  }
  invalidateWindow(key);
  ConvoSummary& c = _index.ensure(key);
  c.count = 0; c.logBytes = 0; c.unread = 0; c.notifyUnread = 0;
  _seq++;
}

void MessageStore::deleteConvo(const ConvoKey& key) {
  if (_backend) {
    char name[15]; codec::keyToName(key, name);
    _backend->remove(name);
  }
  invalidateWindow(key);
  _index.dropByKey(key);
  _seq++;
}

void MessageStore::markUnread(const ConvoKey& key) {
  int ci = _index.find(key);
  if (ci < 0) return;
  ConvoSummary& c = _index.ensure(key);
  if (c.unread) return;
  c.unread = 1; _seq++;
}

void MessageStore::markNotifiable(const ConvoKey& key) {
  int ci = _index.find(key);
  if (ci < 0) return;
  if (_hasActive && _active.equals(key)) return;
  ConvoSummary& c = _index.ensure(key);
  c.notifyUnread++; _seq++;
}

void MessageStore::ensureChannel(uint8_t channelIdx) {
  ConvoKey k = channelKey(channelIdx);
  if (_index.find(k) >= 0) return;
  ensureSlot(k);
  _seq++;
}

// ---- tracked table ----

MessageStore::Tracked* MessageStore::openTracked(const ConvoKey& key, uint32_t senderTime, uint8_t kind) {
  int slot = -1;
  for (int i = 0; i < MAX_TRACKED; i++) if (!_tracked[i].used) { slot = i; break; }
  if (slot < 0) {
    memmove(&_tracked[0], &_tracked[1], sizeof(Tracked) * (MAX_TRACKED - 1));
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
