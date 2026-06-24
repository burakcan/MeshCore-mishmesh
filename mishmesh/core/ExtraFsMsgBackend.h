// mishmesh/core/ExtraFsMsgBackend.h
// Device-only: Arduino + FILESYSTEM (Adafruit_LittleFS on nRF52). Not compiled
// under the native test environment (ExtraFsMsgBackend.cpp is not in the native
// build_src_filter).
#pragma once

#include <mishmesh/core/MsgLogBackend.h>

namespace mishmesh {

// MsgLogBackend over Adafruit_LittleFS (or any FILESYSTEM typedef).
// Each chat log is a file <dir>/<14-hex-name>; the index blob is <dir>/index.
// The dir is created lazily in begin() after the filesystem is mounted.
class ExtraFsMsgBackend : public MsgLogBackend {
public:
  // Default-constructible: call begin(fs) before use.
  ExtraFsMsgBackend() : _fs(nullptr) { _dir[0] = '\0'; }

  // Call once after the FILESYSTEM is mounted (typically in UITask::begin).
  // Creates the directory and configures this backend. dir defaults to "/m".
  void begin(FILESYSTEM& fs, const char* dir = "/m");

  // ---- per-chat log files ----
  bool     append(const char* name, const uint8_t* rec, uint32_t len) override;
  uint32_t size(const char* name) const override;
  uint32_t read(const char* name, uint32_t off, uint8_t* dst, uint32_t cap) const override;
  bool     rewrite(const char* name, const uint8_t* data, uint32_t len) override;
  bool     remove(const char* name) override;

  // ---- in-place mutation ----
  bool patch(const char* name, uint32_t off, const uint8_t* src, uint32_t len) override;
  bool dropFront(const char* name, uint32_t bytes) override;
  bool removeRange(const char* name, uint32_t off, uint32_t len) override;
  bool truncate(const char* name, uint32_t len) override;

  // ---- enumeration ----
  int  list(MsgLogInfo* out, int cap) const override;

  // ---- index blob ----
  bool     saveIndex(const uint8_t* data, uint32_t len) override;
  uint32_t loadIndex(uint8_t* dst, uint32_t cap) const override;

  // ---- capacity ----
  uint32_t freeBytes() const override;

private:
  FILESYSTEM* _fs;
  char        _dir[16];   // e.g. "/m"

  // Stack-local path helpers.
  void makePath(char* buf, int cap, const char* name) const;
  void makeTmpPath(char* buf, int cap, const char* name) const;
  void makeIndexPath(char* buf, int cap) const;

  // Copy bytes [srcOff, srcOff+len) from srcPath to dstPath (append mode).
  // Uses a 256-byte stack buffer; returns false on I/O error.
  bool copyRange(const char* srcPath, uint32_t srcOff, uint32_t len,
                 const char* dstPath) const;
};

} // namespace mishmesh
