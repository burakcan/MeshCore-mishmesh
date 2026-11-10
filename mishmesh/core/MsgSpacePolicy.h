// mishmesh/core/MsgSpacePolicy.h
// How much of a shared filesystem the message logs may take. Pure arithmetic,
// so the numbers for a given board can be checked on the host.
#pragma once
#include <stdint.h>

namespace mishmesh {

// What MessageStore leaves free on its backend. All zero = no limit beyond
// MSG_FLASH_BUDGET and the space physically left.
struct MsgSpacePolicy {
  uint32_t blockSize;  // allocation unit of the backend, 0 = unknown
  uint32_t reserve;    // never taken by an append: other files grow into it
  uint32_t headroom;   // wanted on top for in-place rewrites, given up below `keep`
  uint32_t keep;       // log bytes the store holds on to even without headroom
};

// Flash a file of `bytes` takes on LittleFS v1: whole blocks, each losing about
// 8 bytes to skip-list pointers (4 per pointer, two per block on average).
inline uint32_t lfsFootprint(uint32_t bytes, uint32_t blockSize) {
  if (blockSize <= 8) return bytes;
  uint32_t payload = blockSize - 8;
  return (bytes + payload - 1) / payload * blockSize;
}

// The other tenants of the filesystem the logs live on.
struct SharedFsUse {
  uint32_t blockSize;         // 0 = unknown, no policy
  uint32_t channelsBytes;     // channels file as it is now
  uint32_t channelsMaxBytes;  // ... and with every channel slot written
  uint32_t rewriteBytes;      // largest file that is rewritten in place
};

// Small files (one block each plus a directory entry) that may appear between
// two appends, and what one append costs beyond its own bytes.
static const uint32_t PREFS_SLACK_BLOCKS  = 16;
static const uint32_t APPEND_SLACK_BLOCKS = 4;
// Floor for a partition the other files nearly fill: enough for the latest
// few dozen messages. Small enough that such a log can still be rotated.
static const uint32_t MSG_KEEP_BYTES = 6u * 1024;

// LittleFS v1 has no in-place update: writing into the middle of a file copies
// everything from that point to its end before the old blocks are released. A
// rewrite of the largest such file therefore needs its whole footprint free,
// which is what `headroom` holds. Rotating or patching a log leans on it too.
inline MsgSpacePolicy msgSpacePolicy(const SharedFsUse& u) {
  MsgSpacePolicy p = { 0, 0, 0, 0 };
  if (u.blockSize == 0) return p;
  p.blockSize = u.blockSize;
  uint32_t chanNow = lfsFootprint(u.channelsBytes, u.blockSize);
  uint32_t chanMax = lfsFootprint(u.channelsMaxBytes, u.blockSize);
  p.reserve  = (chanMax > chanNow ? chanMax - chanNow : 0)
             + (PREFS_SLACK_BLOCKS + APPEND_SLACK_BLOCKS) * u.blockSize;
  p.headroom = lfsFootprint(u.rewriteBytes, u.blockSize);
  p.keep     = MSG_KEEP_BYTES;
  return p;
}

// Free space a file needs before it is rewritten from `nowBytes` to
// `nextBytes`. The old copy is removed first, so only growth counts; two
// blocks cover the directory update.
inline uint32_t rewriteGrowth(uint32_t nowBytes, uint32_t nextBytes, uint32_t blockSize) {
  uint32_t now = lfsFootprint(nowBytes, blockSize), next = lfsFootprint(nextBytes, blockSize);
  return (next > now ? next - now : 0) + 2 * blockSize;
}

} // namespace mishmesh
