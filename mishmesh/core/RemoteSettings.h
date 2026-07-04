// mishmesh/core/RemoteSettings.h
#pragma once
#include <stdint.h>
#include <mishmesh/core/PendingRequest.h>

namespace mishmesh {

struct ContactsService;

// Descriptor for one remote (CLI-backed) config field. getCmd null => not fetched
// (write-only, e.g. passwords). setCmd null => read-only (no editor, never saved).
struct SettingFieldDef {
  const char* label;
  const char* getCmd;
  const char* setCmd;
  enum Kind : uint8_t { Text, Number, Float, Toggle, ReadOnly } kind;
  int32_t minVal, maxVal;   // Number bounds (informational; editor may clamp); else unused
  const char* okPrefix;     // save-success reply prefix; null => "OK"
  bool longValue;           // ReadOnly field whose value is long (e.g. pubkey): list shows
                            // a truncated preview and Select opens a ScrollText modal
};

// Drives the CLI get/set choreography for a field table over ContactsService.
// Operates on panel-owned arrays (no heap): a flat `staged` buffer of n*stride bytes
// (field i's value = staged + i*stride) plus dirty[n]/fetched[n]. Reuses PendingRequest.
class RemoteSettingsEngine {
public:
  enum class Phase : uint8_t { Idle, Fetching, Ready, Saving, Error };

  uint32_t timeoutMs = 20000;   // per-field CLI round-trip

  void configure(ContactsService* svc, const uint8_t* pub6,
                 const SettingFieldDef* defs, int n,
                 char* staged, uint16_t stride, bool* dirty, bool* fetched);
  void beginFetch(uint32_t nowMs);   // resets dirty/fetched; fetches each field in order
  void beginSave(uint32_t nowMs);    // sets each dirty field in order (halt on first error)
  void poll(uint32_t seqNow, uint32_t nowMs);   // call each onRender

  Phase phase() const { return _phase; }
  int   activeIndex() const { return _idx; }
  int   errorIndex() const { return _errIdx; }
  int   fieldCount() const { return _n; }
  bool  anyDirty() const;
  char*       value(int i)       { return _staged + (uint32_t)i * _stride; }
  const char* value(int i) const { return _staged + (uint32_t)i * _stride; }

private:
  ContactsService* _svc = nullptr;
  const uint8_t*   _pub = nullptr;
  const SettingFieldDef* _defs = nullptr;
  int      _n = 0, _idx = -1, _errIdx = -1;
  char*    _staged = nullptr;
  uint16_t _stride = 0;
  bool*    _dirty = nullptr;
  bool*    _fetched = nullptr;
  Phase    _phase = Phase::Idle;
  PendingRequest _req;
  uint32_t _startSeq = 0;

  int  nextFetchIdx(int from) const;   // next i>=from with non-null getCmd (-1 none)
  int  nextSaveIdx(int from) const;    // next i>=from with dirty && setCmd (-1 none)
  void fireGet(uint32_t nowMs);
  void fireSet(uint32_t nowMs);
};

}  // namespace mishmesh
