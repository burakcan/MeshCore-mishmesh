// mishmesh/core/ExtraFsMsgBackend.cpp
// Device-only: do NOT add to the native build_src_filter.

// Include platform Arduino + LittleFS headers BEFORE ExtraFsMsgBackend.h so that
// the FILESYSTEM typedef and File class are visible when the header is parsed.
#include <Arduino.h>
#include <string.h>
#include "PersistDebug.h"   // [mishmesh] TEMP: unread-persistence instrumentation

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <Adafruit_LittleFS.h>
  using namespace Adafruit_LittleFS_Namespace;
  #ifndef FILESYSTEM
    #define FILESYSTEM Adafruit_LittleFS
  #endif
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
  #ifndef FILESYSTEM
    #define FILESYSTEM fs::FS
  #endif
#elif defined(ESP32)
  #include <SPIFFS.h>
  #ifndef FILESYSTEM
    #define FILESYSTEM fs::FS
  #endif
#endif

#include <mishmesh/core/ExtraFsMsgBackend.h>

// Adafruit_LittleFS (nRF52/STM32) exposes File::open()/isOpen(), so one File can
// be kept open and reused across reads. Other backends (RP2040/ESP32 fs::File)
// don't, so they stay on the open-per-call path below.
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #define MSG_FS_READ_CACHE 1
  #include <new>   // placement new for the statically-stored handle
#endif

namespace mishmesh {

// ---- helpers ----

static const int CHUNK = 256;   // stack buffer size for copy-and-swap

namespace {

// Opening a per-chat log costs ~tens of ms on LittleFS (directory + metadata
// walk). A single chat open issues ~250 same-file read()/size() calls (window
// load + full-thread height remeasure), so reopening per call dominated the
// 6-7 s freeze. We keep ONE read handle open and reuse it while the caller keeps
// hitting the same file; any write (or a different file) closes it first, so a
// read handle is never live while the same file is mutated; LittleFS single-
// writer safety is preserved. ExtraFsMsgBackend is a singleton on device, so
// file-scope state is safe and avoids leaking the platform File type into the
// header.
uint32_t rawSize(FILESYSTEM* fs, const char* dir, const char* name) {
  if (!fs) return 0;
  char path[32]; snprintf(path, sizeof(path), "%s/%s", dir, name);
  File f = fs->open(path, FILE_O_READ);
  if (!f) return 0;
  uint32_t sz = f.size();
  f.close();
  return sz;
}

#ifdef MSG_FS_READ_CACHE
alignas(File) uint8_t s_rfStore[sizeof(File)];
File* s_rf = nullptr;        // lazily placement-constructed, bound to the fs
char  s_rfName[15] = {0};    // log the handle is open on ("" = none)

void closeReadCache() {
  if (s_rf && s_rf->isOpen()) s_rf->close();
  s_rfName[0] = '\0';
}

// Open (or reuse) the cached read handle on <dir>/<name>. nullptr if absent.
File* acquireRead(FILESYSTEM* fs, const char* dir, const char* name) {
  if (!fs) return nullptr;
  if (!s_rf) s_rf = new (s_rfStore) File(*fs);
  if (s_rfName[0] && strcmp(s_rfName, name) != 0) closeReadCache();
  if (!s_rf->isOpen()) {
    char path[32]; snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!s_rf->open(path, FILE_O_READ)) { s_rfName[0] = '\0'; return nullptr; }
    strncpy(s_rfName, name, sizeof(s_rfName) - 1);
    s_rfName[sizeof(s_rfName) - 1] = '\0';
  }
  return s_rf;
}
#else
inline void closeReadCache() {}
#endif

}  // namespace

void ExtraFsMsgBackend::begin(FILESYSTEM& fs, const char* dir) {
  _fs = &fs;
  strncpy(_dir, dir ? dir : "/m", sizeof(_dir) - 1);
  _dir[sizeof(_dir) - 1] = '\0';
  _fs->mkdir(_dir);
}

void ExtraFsMsgBackend::makePath(char* buf, int cap, const char* name) const {
  snprintf(buf, cap, "%s/%s", _dir, name);
}

void ExtraFsMsgBackend::makeTmpPath(char* buf, int cap, const char* name) const {
  snprintf(buf, cap, "%s/%s.t", _dir, name);
}

void ExtraFsMsgBackend::makeIndexPath(char* buf, int cap) const {
  snprintf(buf, cap, "%s/index", _dir);
}

bool ExtraFsMsgBackend::copyRange(const char* srcPath, uint32_t srcOff, uint32_t len,
                                   const char* dstPath) const {
  File src = _fs->open(srcPath, FILE_O_READ);
  if (!src) return false;
  if (!src.seek(srcOff)) { src.close(); return false; }

  File dst = _fs->open(dstPath, FILE_O_WRITE);
  if (!dst) { src.close(); return false; }

  alignas(uint32_t) uint8_t chunk[CHUNK];   // word-aligned: LittleFS progs 256B chunks
  uint32_t remaining = len;                 // directly from here, nRF52 QSPI DMA needs alignment
  bool ok = true;
  while (remaining > 0 && ok) {
    uint32_t want = remaining < CHUNK ? remaining : CHUNK;
    int got = src.read(chunk, (uint16_t)want);
    if (got <= 0) break;
    size_t written = dst.write(chunk, (size_t)got);
    if (written != (size_t)got) { ok = false; break; }
    remaining -= (uint32_t)got;
  }
  src.close();
  dst.close();
  return ok && remaining == 0;
}

// ---- per-chat log files ----

bool ExtraFsMsgBackend::append(const char* name, const uint8_t* rec, uint32_t len) {
  if (!_fs) return false;
  closeReadCache();   // no read handle may be open while we write this log
  char path[32]; makePath(path, sizeof(path), name);
  File f = _fs->open(path, FILE_O_WRITE);   // RDWR|CREAT, seeks to end
  if (!f) return false;
  size_t w = f.write(rec, len);
  f.close();
  if (w != (size_t)len)
    MM_PLOG("fs.append: WRITE FAILED name=%s len=%lu wrote=%lu", name, (unsigned long)len, (unsigned long)w);
  return w == (size_t)len;
}

uint32_t ExtraFsMsgBackend::size(const char* name) const {
  if (!_fs) return 0;
#ifdef MSG_FS_READ_CACHE
  File* f = acquireRead(_fs, _dir, name);
  return f ? f->size() : 0;
#else
  return rawSize(_fs, _dir, name);
#endif
}

uint32_t ExtraFsMsgBackend::read(const char* name, uint32_t off,
                                  uint8_t* dst, uint32_t cap) const {
  if (!_fs) return 0;
#ifdef MSG_FS_READ_CACHE
  File* f = acquireRead(_fs, _dir, name);
  if (!f) return 0;
  if (!f->seek(off)) return 0;   // handle stays open for the next read
  int n = f->read(dst, (uint16_t)(cap > 65535u ? 65535u : cap));
  return n > 0 ? (uint32_t)n : 0;
#else
  char path[32]; makePath(path, sizeof(path), name);
  File f = _fs->open(path, FILE_O_READ);
  if (!f) return 0;
  if (!f.seek(off)) { f.close(); return 0; }
  int n = f.read(dst, (uint16_t)(cap > 65535u ? 65535u : cap));
  f.close();
  return n > 0 ? (uint32_t)n : 0;
#endif
}

bool ExtraFsMsgBackend::rewrite(const char* name, const uint8_t* data, uint32_t len) {
  if (!_fs) return false;
  closeReadCache();
  char path[32]; makePath(path, sizeof(path), name);
  _fs->remove(path);
  // Fix 3: always create the file (even for len==0) so the file is PRESENT but
  // empty, matching FakeMsgLogBackend where files[name].assign(...,0) keeps the key.
  File f = _fs->open(path, FILE_O_WRITE);
  if (!f) return false;
  if (len > 0 && data != nullptr) {
    size_t w = f.write(data, (size_t)len);
    f.close();
    return w == (size_t)len;
  }
  f.close();   // 0-byte file, leave present
  return true;
}

bool ExtraFsMsgBackend::remove(const char* name) {
  if (!_fs) return false;
  closeReadCache();
  char path[32]; makePath(path, sizeof(path), name);
  _fs->remove(path);
  return true;
}

// ---- in-place mutation ----

bool ExtraFsMsgBackend::patch(const char* name, uint32_t off,
                               const uint8_t* src, uint32_t len) {
  if (!_fs) return false;
  closeReadCache();
  char path[32]; makePath(path, sizeof(path), name);
  // FILE_O_WRITE = LFS_O_RDWR|LFS_O_CREAT; seeks to end. Seek back to off.
  File f = _fs->open(path, FILE_O_WRITE);
  if (!f) return false;
  if (!f.seek(off)) { f.close(); return false; }
  size_t w = f.write(src, (size_t)len);
  f.close();
  return w == (size_t)len;
}

bool ExtraFsMsgBackend::dropFront(const char* name, uint32_t bytes) {
  if (!_fs) return false;
  closeReadCache();
  char path[32];    makePath(path, sizeof(path), name);
  char tmp[36];     makeTmpPath(tmp, sizeof(tmp), name);

  uint32_t sz = rawSize(_fs, _dir, name);
  if (bytes == 0) return true;
  if (bytes >= sz) {
    // Fix 3: leave a PRESENT 0-byte file (match FakeMsgLogBackend: f.clear() keeps key).
    File f = _fs->open(path, FILE_O_WRITE);
    if (f) { f.truncate(0); f.close(); }
    return true;
  }

  // Copy the tail [bytes, sz) to tmp, then swap.
  _fs->remove(tmp);   // clear any stale tmp
  bool ok = copyRange(path, bytes, sz - bytes, tmp);
  if (!ok) { _fs->remove(tmp); return false; }
  // Fix 1: lfs_rename atomically replaces an existing destination, no explicit
  // remove(path) needed. The remove-then-rename window risked total log loss on
  // power failure between the two calls.
  _fs->rename(tmp, path);
  return true;
}

bool ExtraFsMsgBackend::removeRange(const char* name, uint32_t off, uint32_t len) {
  if (!_fs) return false;
  closeReadCache();
  char path[32];    makePath(path, sizeof(path), name);
  char tmp[36];     makeTmpPath(tmp, sizeof(tmp), name);

  uint32_t sz = rawSize(_fs, _dir, name);
  if (off >= sz || len == 0) return true;
  uint32_t end = off + len;
  if (end > sz) end = sz;

  _fs->remove(tmp);
  // Fix 3: pre-create tmp as an empty file so rename always has a target, even
  // when the removed range spans the whole file (both copy conditions are false).
  // This also matches FakeMsgLogBackend where a full-span erase leaves a 0-byte key.
  { File f = _fs->open(tmp, FILE_O_WRITE); if (f) f.close(); }

  // Copy [0, off) to tmp.
  bool ok = true;
  if (off > 0) ok = copyRange(path, 0, off, tmp);

  // Append [end, sz) to tmp.
  if (ok && end < sz) {
    File src = _fs->open(path, FILE_O_READ);
    if (!src) { ok = false; }
    else {
      File dst = _fs->open(tmp, FILE_O_WRITE);   // opens existing, seeks to end
      if (!dst) { src.close(); ok = false; }
      else {
        if (!src.seek(end)) ok = false;
        else {
          alignas(uint32_t) uint8_t chunk[CHUNK];   // word-aligned for direct QSPI DMA progs
          uint32_t remaining = sz - end;
          while (remaining > 0 && ok) {
            uint32_t want = remaining < CHUNK ? remaining : CHUNK;
            int got = src.read(chunk, (uint16_t)want);
            if (got <= 0) { ok = false; break; }
            size_t w = dst.write(chunk, (size_t)got);
            if (w != (size_t)got) { ok = false; break; }
            remaining -= (uint32_t)got;
          }
        }
        dst.close();
      }
      src.close();
    }
  }

  if (!ok) { _fs->remove(tmp); return false; }
  // Fix 1: lfs_rename atomically replaces an existing destination, no explicit
  // remove(path) before rename. Removing path first opens a power-loss window
  // that would destroy the log.
  _fs->rename(tmp, path);
  return true;
}

bool ExtraFsMsgBackend::truncate(const char* name, uint32_t len) {
  if (!_fs) return false;
  closeReadCache();
  char path[32]; makePath(path, sizeof(path), name);
  uint32_t curSize = rawSize(_fs, _dir, name);
  if (curSize <= len) return true;   // already short enough
  File f = _fs->open(path, FILE_O_WRITE);   // RDWR|CREAT, seeks to end
  if (!f) return false;
  bool ok = f.truncate(len);
  f.close();
  return ok;
}

// ---- enumeration ----

int ExtraFsMsgBackend::list(MsgLogInfo* out, int cap) const {
  if (!_fs) return 0;
  closeReadCache();   // don't hold a log handle open across a directory traversal
  File dir = _fs->open(_dir, FILE_O_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return 0;
  }
  int n = 0;
  while (n < cap) {
    File f = dir.openNextFile(FILE_O_READ);
    if (!f) break;
    const char* fname = f.name();
    // Only include 14-char hex log files; skip "index" and tmp files.
    if (fname && strlen(fname) == 14) {
      strncpy(out[n].name, fname, 14);
      out[n].name[14] = '\0';
      out[n].bytes = f.size();
      n++;
    }
    f.close();
  }
  dir.close();
  return n;
}

// ---- index blob ----

bool ExtraFsMsgBackend::saveIndex(const uint8_t* data, uint32_t len) {
  if (!_fs) { MM_PLOG("fs.saveIndex: no _fs"); return false; }
  closeReadCache();
  char path[32]; makeIndexPath(path, sizeof(path));
  _fs->remove(path);
  if (len == 0) return true;
  File f = _fs->open(path, FILE_O_WRITE);
  if (!f) { MM_PLOG("fs.saveIndex: open FAILED path=%s", path); return false; }
  size_t w = f.write(data, (size_t)len);
  f.close();
  MM_PLOG("fs.saveIndex: path=%s len=%lu wrote=%lu", path, (unsigned long)len, (unsigned long)w);
  return w == (size_t)len;
}

uint32_t ExtraFsMsgBackend::loadIndex(uint8_t* dst, uint32_t cap) const {
  if (!_fs) { MM_PLOG("fs.loadIndex: no _fs"); return 0; }
  char path[32]; makeIndexPath(path, sizeof(path));
  File f = _fs->open(path, FILE_O_READ);
  if (!f) { MM_PLOG("fs.loadIndex: open FAILED (no file) path=%s", path); return 0; }
  int n = f.read(dst, (uint16_t)(cap > 65535u ? 65535u : cap));
  f.close();
  MM_PLOG("fs.loadIndex: path=%s read=%d", path, n);
  return n > 0 ? (uint32_t)n : 0;
}

// ---- capacity ----

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
// Fix 2: lfs_traverse can visit a block more than once, so the running count can
// exceed block_count and make usedBytes > totalBytes. Cap by aborting with
// LFS_ERR_CORRUPT once the count reaches the limit (same guard as DataStore).
struct _CountBlockCtx { lfs_size_t used; lfs_size_t total; };
static int _countBlock(void* p, lfs_block_t /*block*/) {
  _CountBlockCtx* ctx = (_CountBlockCtx*)p;
  if (ctx->used >= ctx->total) return LFS_ERR_CORRUPT;
  ctx->used++;
  return 0;
}
#endif

uint32_t ExtraFsMsgBackend::freeBytes() const {
  if (!_fs) return 0;
  closeReadCache();   // lfs_traverse walks all blocks; don't leave a file open
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  lfs_t* lfs = _fs->_getFS();
  if (!lfs || !lfs->cfg) return 0;
  _CountBlockCtx ctx;
  ctx.used  = 0;
  ctx.total = (lfs_size_t)lfs->cfg->block_count;
  if (lfs_traverse(lfs, _countBlock, &ctx) < 0) return 0;
  uint32_t blockSize  = (uint32_t)lfs->cfg->block_size;
  uint32_t blockCount = (uint32_t)lfs->cfg->block_count;
  uint32_t usedBytes  = (uint32_t)ctx.used * blockSize;
  uint32_t totalBytes = blockCount * blockSize;
  return usedBytes < totalBytes ? totalBytes - usedBytes : 0;
#else
  return 256u * 1024;   // fallback for non-nRF52 device builds
#endif
}

} // namespace mishmesh
