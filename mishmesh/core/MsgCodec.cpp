// mishmesh/core/MsgCodec.cpp
#include "MsgCodec.h"
#include <string.h>

namespace mishmesh { namespace codec {

uint8_t  recKind(const uint8_t* r)    { return (r[0] >> 3) & 0x03; }
bool     recDead(const uint8_t* r)    { return r[0] & 0x04; }
uint16_t recTextLen(const uint8_t* r) { return r[16]; }

int recSize(const uint8_t* r) {
  int n = REC_HDR + recTextLen(r);
  switch (recKind(r)) {
    case KIND_INBOUND:  n += 2 + pathByteLen(r[n + 1]); break; // snrx4+encoded path len+path
    case KIND_OUT_DM:   n += 3; break;               // status(1)+trip(2)
    case KIND_OUT_CHAN: n += 2; break;               // status(1)+heard(1)
  }
  return n;
}

void rdKey(const uint8_t* r, ConvoKey& k) { k.type = r[1]; memcpy(k.id, r + 2, 6); }
uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
void     wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }

int writeRecord(uint8_t* dst, uint8_t kind, const ConvoKey& key,
                const char* text, uint16_t textLen,
                uint32_t senderTime, uint32_t localTime,
                const uint8_t* trailer, uint8_t trailerLen) {
  if (textLen > MAX_TEXT) textLen = MAX_TEXT;
  dst[0] = (uint8_t)((kind & 0x03) << 3) | (key.type == 1 ? 0x02 : 0) | (kind != KIND_INBOUND ? 0x01 : 0);
  dst[1] = key.type; memcpy(dst + 2, key.id, 6);
  wr32(dst + 8, senderTime); wr32(dst + 12, localTime);
  dst[16] = (uint8_t)textLen; memcpy(dst + 17, text, textLen);
  memcpy(dst + REC_HDR + textLen, trailer, trailerLen);
  return REC_HDR + textLen + trailerLen;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

void keyToName(const ConvoKey& key, char out[15]) {
  static const char hex[] = "0123456789abcdef";
  out[0] = hex[(key.type >> 4) & 0xF];
  out[1] = hex[key.type & 0xF];
  for (int i = 0; i < 6; i++) {
    out[2 + i * 2]     = hex[(key.id[i] >> 4) & 0xF];
    out[2 + i * 2 + 1] = hex[key.id[i] & 0xF];
  }
  out[14] = '\0';
}

bool nameToKey(const char* name, ConvoKey& key) {
  if (strlen(name) != 14) return false;
  int hi = hexVal(name[0]), lo = hexVal(name[1]);
  if (hi < 0 || lo < 0) return false;
  key.type = (uint8_t)((hi << 4) | lo);
  for (int i = 0; i < 6; i++) {
    hi = hexVal(name[2 + i * 2]);
    lo = hexVal(name[2 + i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    key.id[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

}} // namespace mishmesh::codec
