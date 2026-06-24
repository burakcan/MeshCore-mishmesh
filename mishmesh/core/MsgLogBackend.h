// mishmesh/core/MsgLogBackend.h
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace mishmesh {

// One log file's identity for index rebuild / eviction.
struct MsgLogInfo {
  char     name[15];   // "<14 hex>" NUL-terminated (raw ConvoKey bytes as hex)
  uint32_t bytes;      // current file size
};

// All persistence goes through this. The store never touches a filesystem
// directly, so its logic stays host-testable (FakeMsgLogBackend) and Arduino-free.
// `name` is always the 14-hex filename produced by keyToName() (see MsgCodec).
struct MsgLogBackend {
  virtual ~MsgLogBackend() {}

  // ---- per-chat log files ----
  virtual bool     append(const char* name, const uint8_t* rec, uint32_t len) = 0;
  virtual uint32_t size(const char* name) const = 0;                       // 0 if absent
  virtual uint32_t read(const char* name, uint32_t off, uint8_t* dst, uint32_t cap) const = 0; // bytes read
  virtual bool     rewrite(const char* name, const uint8_t* data, uint32_t len) = 0; // replace whole file
  virtual bool     remove(const char* name) = 0;

  // ---- in-place mutation (buffer-free; device backend implements over LittleFS) ----
  virtual bool patch(const char* name, uint32_t off, const uint8_t* src, uint32_t len) = 0;
  // overwrite len bytes in place starting at off
  virtual bool dropFront(const char* name, uint32_t bytes) = 0;
  // remove the leading `bytes` bytes (rotation)
  virtual bool removeRange(const char* name, uint32_t off, uint32_t len) = 0;
  // remove the span [off, off+len) (delete-message)
  virtual bool truncate(const char* name, uint32_t len) = 0;
  // shrink the file to `len` bytes; no-op if already <= len (torn-tail heal)

  // ---- enumeration (index rebuild + backstop) ----
  virtual int      list(MsgLogInfo* out, int cap) const = 0;               // count written

  // ---- index blob ----
  virtual bool     saveIndex(const uint8_t* data, uint32_t len) = 0;
  virtual uint32_t loadIndex(uint8_t* dst, uint32_t cap) const = 0;        // bytes read, 0 if absent

  // ---- capacity (backstop) ----
  virtual uint32_t freeBytes() const = 0;                                  // remaining space
};

} // namespace mishmesh
