#pragma once

#include <stdint.h>

namespace mishmesh {

// Generic small-blob persistence, keyed by a short string. The framework knows only
// "applets can persist bytes by key" - never what any applet stores. Implemented by the
// adapter (UITask) over the device filesystem; a null AppletContext::storage means
// "no persistence available" and callers must tolerate load()==0 / save() no-op.
// Keys must be short (the adapter builds a "/mm/<key>" path in a fixed buffer - keep
// keys <= ~32 chars); blobs are small (the adapter caps them, currently 128 bytes).
struct AppletStorage {
  virtual ~AppletStorage() {}
  // Reads up to `cap` bytes for `key` into `dst`; returns bytes read, 0 if absent.
  virtual uint8_t load(const char* key, uint8_t* dst, uint8_t cap) = 0;
  // Persists `len` bytes for `key`; returns false on failure.
  virtual bool save(const char* key, const uint8_t* src, uint8_t len) = 0;
};

}  // namespace mishmesh
