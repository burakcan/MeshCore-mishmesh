// mishmesh/core/ConvoIndex.cpp
#include "ConvoIndex.h"
#include <string.h>

namespace mishmesh {

void ConvoIndex::reset() {
  _count = 0;
  memset(_convos, 0, sizeof(_convos));
}

int ConvoIndex::find(const ConvoKey& key) const {
  for (int i = 0; i < _count; i++) if (_convos[i].key.equals(key)) return i;
  return -1;
}

ConvoSummary& ConvoIndex::ensure(const ConvoKey& key) {
  int ci = find(key);
  if (ci >= 0) return _convos[ci];
  if (_count >= MAX_CONVOS) {
    ConvoKey victim;
    evictLRU(victim);   // caller is responsible for deleting the victim's log
  }
  ci = _count++;
  ConvoSummary& s = _convos[ci];
  s.key          = key;
  s.lastTime     = 0;
  s.unread       = 0;
  s.notifyUnread = 0;
  s.count        = 0;
  s.logBytes     = 0;
  s.preview[0]   = 0;
  return s;
}

bool ConvoIndex::evictLRU(ConvoKey& victimOut) {
  if (_count == 0) return false;
  int lru = 0;
  for (int i = 1; i < _count; i++)
    if (_convos[i].lastTime < _convos[lru].lastTime) lru = i;
  victimOut = _convos[lru].key;
  for (int i = lru; i < _count - 1; i++) _convos[i] = _convos[i + 1];
  _count--;
  return true;
}

ConvoKey ConvoIndex::lruKey() const {
  int lru = 0;
  for (int i = 1; i < _count; i++)
    if (_convos[i].lastTime < _convos[lru].lastTime) lru = i;
  return _convos[lru].key;
}

bool ConvoIndex::get(int rankIndex, ConvoSummary& out) const {
  if (rankIndex < 0 || rankIndex >= _count) return false;
  // selection-rank by lastTime descending without mutating _convos
  int rank = 0, best = -1; uint32_t bestTime = 0; bool bestSet = false;
  for (int pass = 0; pass <= rankIndex; pass++) {
    best = -1; bestSet = false;
    for (int i = 0; i < _count; i++) {
      uint32_t t = _convos[i].lastTime;
      if (!bestSet || t > bestTime || (t == bestTime && i > best)) {
        bool chosen = false;
        int greater = 0;
        for (int j = 0; j < _count; j++) {
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

bool ConvoIndex::dropByKey(const ConvoKey& key) {
  int ci = find(key);
  if (ci < 0) return false;
  for (int i = ci; i < _count - 1; i++) _convos[i] = _convos[i + 1];
  _count--;
  return true;
}

void ConvoIndex::touch(int ci, uint32_t time) {
  // lastTime drives recency sort and LRU eviction, never shown directly.
  // Always make the touched convo strictly newest to preserve receive order
  // even when RTC is unset or sender clocks are skewed.
  uint32_t maxT = 0;
  for (int i = 0; i < _count; i++) if (_convos[i].lastTime > maxT) maxT = _convos[i].lastTime;
  uint32_t nt = time > maxT ? time : maxT + 1;
  if (nt > _convos[ci].lastTime) _convos[ci].lastTime = nt;
}

void ConvoIndex::setPreview(const ConvoKey& key, const char* text, uint16_t len) {
  int ci = find(key);
  if (ci < 0) return;
  uint16_t cap = (uint16_t)(PREVIEW_LEN - 1);
  if (len > cap) len = cap;
  memcpy(_convos[ci].preview, text, len);
  _convos[ci].preview[len] = 0;
}

const char* ConvoIndex::preview(const ConvoKey& key) const {
  int ci = find(key);
  if (ci < 0) return nullptr;
  return _convos[ci].preview;
}

uint16_t ConvoIndex::rawCount(int ci) const {
  if (ci < 0 || ci >= _count) return 0;
  return _convos[ci].count;
}

uint16_t ConvoIndex::totalUnread() const {
  uint32_t t = 0;
  for (int i = 0; i < _count; i++) t += _convos[i].unread;
  return t > 0xFFFF ? 0xFFFF : (uint16_t)t;
}

uint16_t ConvoIndex::totalNotifyUnread() const {
  uint32_t t = 0;
  for (int i = 0; i < _count; i++) t += _convos[i].notifyUnread;
  return t > 0xFFFF ? 0xFFFF : (uint16_t)t;
}

uint32_t ConvoIndex::totalLogBytes() const {
  uint32_t t = 0;
  for (int i = 0; i < _count; i++) t += _convos[i].logBytes;
  return t;
}

// Per-convo packed record layout: type(1)+id(6)+lastTime(4)+unread(2)+notifyUnread(2)+count(2)+logBytes(4)+preview(PREVIEW_LEN)
static const uint32_t MIDX_PER_CONVO = 1 + 6 + 4 + 2 + 2 + 2 + 4 + PREVIEW_LEN;  // 61

uint32_t ConvoIndex::serialize(uint8_t* out, uint32_t cap) const {
  uint32_t need = 4 + 1 + 2 + (uint32_t)_count * MIDX_PER_CONVO;
  if (cap < need) return 0;
  uint32_t i = 0;
  memcpy(out + i, "MIDX", 4); i += 4;
  out[i++] = 1;   // version
  uint16_t cc = (uint16_t)_count;
  memcpy(out + i, &cc, 2); i += 2;
  for (int c = 0; c < _count; c++) {
    const ConvoSummary& s = _convos[c];
    out[i++] = s.key.type;
    memcpy(out + i, s.key.id, 6);       i += 6;
    memcpy(out + i, &s.lastTime, 4);    i += 4;
    memcpy(out + i, &s.unread, 2);      i += 2;
    memcpy(out + i, &s.notifyUnread, 2); i += 2;
    memcpy(out + i, &s.count, 2);       i += 2;
    memcpy(out + i, &s.logBytes, 4);    i += 4;
    memcpy(out + i, s.preview, PREVIEW_LEN); i += PREVIEW_LEN;
  }
  return i;
}

bool ConvoIndex::deserialize(const uint8_t* in, uint32_t len) {
  if (len < 7) return false;   // 4 magic + 1 version + 2 count
  if (memcmp(in, "MIDX", 4) != 0) return false;
  if (in[4] != 1) return false;
  uint32_t i = 5;
  uint16_t cc;
  memcpy(&cc, in + i, 2); i += 2;
  if (cc > (uint16_t)MAX_CONVOS) return false;
  if (len < i + (uint32_t)cc * MIDX_PER_CONVO) return false;
  reset();
  _count = (int)cc;
  for (int c = 0; c < (int)cc; c++) {
    ConvoSummary& s = _convos[c];
    s.key.type = in[i++];
    memcpy(s.key.id, in + i, 6);        i += 6;
    memcpy(&s.lastTime, in + i, 4);     i += 4;
    memcpy(&s.unread, in + i, 2);       i += 2;
    memcpy(&s.notifyUnread, in + i, 2); i += 2;
    memcpy(&s.count, in + i, 2);        i += 2;
    memcpy(&s.logBytes, in + i, 4);     i += 4;
    memcpy(s.preview, in + i, PREVIEW_LEN); i += PREVIEW_LEN;
    s.preview[PREVIEW_LEN - 1] = 0;    // guard NUL even if blob was corrupt
  }
  return true;
}

} // namespace mishmesh
