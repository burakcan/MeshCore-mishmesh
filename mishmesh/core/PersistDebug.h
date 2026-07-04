// mishmesh/core/PersistDebug.h
// [mishmesh] TEMPORARY instrumentation for the unread-persistence investigation.
// Enable per-env with -D MM_PERSIST_DEBUG=1; prints to Serial (USB). No-op on host/
// native builds (no ARDUINO), so unit tests are unaffected. Remove once diagnosed.
#pragma once
#if defined(MM_PERSIST_DEBUG) && defined(ARDUINO)
  #include <Arduino.h>
  #define MM_PLOG(F, ...) Serial.printf("[mmpersist] " F "\n", ##__VA_ARGS__)
#else
  #define MM_PLOG(...) do {} while (0)
#endif
