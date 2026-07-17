// mishmesh/core/MsgCodec.h
#pragma once
#include <stdint.h>
#include "MsgTypes.h"   // ConvoKey, MsgKind (split out to avoid circular deps)

namespace mishmesh { namespace codec {

constexpr int REC_HDR = 17;                  // flags+type+id(6)+senderTime(4)+localTime(4)+textLen(1)
constexpr int MAX_REC = REC_HDR + MAX_TEXT + 2 + MAX_PATH_BYTES;

uint8_t  recKind(const uint8_t* r);
bool     recDead(const uint8_t* r);
uint16_t recTextLen(const uint8_t* r);
int      recSize(const uint8_t* r);
void     rdKey(const uint8_t* r, ConvoKey& k);
uint32_t rd32(const uint8_t* p);
void     wr32(uint8_t* p, uint32_t v);

// Packs header+text+trailer into dst; trailer bytes (already built by caller) are
// copied verbatim after the text. Returns total length. Caller ensures MAX_REC cap.
int writeRecord(uint8_t* dst, uint8_t kind, const ConvoKey& key,
                const char* text, uint16_t textLen,
                uint32_t senderTime, uint32_t localTime,
                const uint8_t* trailer, uint8_t trailerLen);

void keyToName(const ConvoKey& key, char out[15]);
bool nameToKey(const char* name, ConvoKey& key);

}} // namespace mishmesh::codec
