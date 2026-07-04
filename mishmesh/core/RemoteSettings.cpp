// mishmesh/core/RemoteSettings.cpp
#include <mishmesh/core/RemoteSettings.h>
#include <mishmesh/core/ContactsService.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

void RemoteSettingsEngine::configure(ContactsService* svc, const uint8_t* pub6,
                                     const SettingFieldDef* defs, int n,
                                     char* staged, uint16_t stride, bool* dirty, bool* fetched) {
  _svc = svc; _pub = pub6; _defs = defs; _n = n;
  _staged = staged; _stride = stride; _dirty = dirty; _fetched = fetched;
  _phase = Phase::Idle; _idx = -1; _errIdx = -1;
}

int RemoteSettingsEngine::nextFetchIdx(int from) const {
  for (int i = from; i < _n; i++) if (_defs[i].getCmd) return i;
  return -1;
}
int RemoteSettingsEngine::nextSaveIdx(int from) const {
  for (int i = from; i < _n; i++) if (_dirty[i] && _defs[i].setCmd) return i;
  return -1;
}
bool RemoteSettingsEngine::anyDirty() const {
  for (int i = 0; i < _n; i++) if (_dirty[i] && _defs[i].setCmd) return true;
  return false;
}

void RemoteSettingsEngine::fireGet(uint32_t nowMs) {
  _startSeq = _svc ? _svc->cliSeq() : 0;
  _req.timeoutMs = timeoutMs;
  _req.begin(_startSeq, nowMs);
  if (_svc) _svc->sendCliCommand(_pub, _defs[_idx].getCmd);
}

void RemoteSettingsEngine::fireSet(uint32_t nowMs) {
  char cmd[176];
  snprintf(cmd, sizeof(cmd), "%s %s", _defs[_idx].setCmd, value(_idx));
  _startSeq = _svc ? _svc->cliSeq() : 0;
  _req.timeoutMs = timeoutMs;
  _req.begin(_startSeq, nowMs);
  if (_svc) _svc->sendCliCommand(_pub, cmd);
}

void RemoteSettingsEngine::beginFetch(uint32_t nowMs) {
  for (int i = 0; i < _n; i++) {
    _dirty[i] = false;
    // Write-only (null getCmd) fields have nothing to fetch: mark done + blank.
    if (_defs[i].getCmd) { _fetched[i] = false; }
    else { _fetched[i] = true; value(i)[0] = 0; }
  }
  _idx = nextFetchIdx(0);
  if (_idx < 0) { _phase = Phase::Ready; return; }
  _phase = Phase::Fetching;
  fireGet(nowMs);
}

void RemoteSettingsEngine::beginSave(uint32_t nowMs) {
  _errIdx = -1;
  _idx = nextSaveIdx(0);
  if (_idx < 0) { _phase = Phase::Ready; return; }
  _phase = Phase::Saving;
  fireSet(nowMs);
}

void RemoteSettingsEngine::poll(uint32_t seqNow, uint32_t nowMs) {
  if (_phase != Phase::Fetching && _phase != Phase::Saving) return;
  PendingRequest::State st = _req.poll(seqNow, nowMs);

  if (_phase == Phase::Fetching) {
    if (st == PendingRequest::State::Ready) {
      bool ok = false; const char* resp = nullptr;
      if (_svc && _svc->cliResult(_pub, _startSeq, ok, resp)) {
        const char* v = resp ? resp : "";
        if (v[0] == '>' && v[1] == ' ') v += 2;   // strip the "> " prefix if present
        strncpy(value(_idx), v, _stride - 1);
        value(_idx)[_stride - 1] = 0;
        _fetched[_idx] = true;
        _idx = nextFetchIdx(_idx + 1);
        if (_idx < 0) _phase = Phase::Ready; else fireGet(nowMs);
      } else {
        _req.rearm(seqNow);   // reply was for a different contact
      }
    } else if (st == PendingRequest::State::TimedOut) {
      strncpy(value(_idx), "?", _stride - 1); value(_idx)[_stride - 1] = 0;
      _fetched[_idx] = true;
      _idx = nextFetchIdx(_idx + 1);
      if (_idx < 0) _phase = Phase::Ready; else fireGet(nowMs);
    }
    return;
  }

  // Saving
  if (st == PendingRequest::State::Ready) {
    bool ok = false; const char* resp = nullptr;
    if (_svc && _svc->cliResult(_pub, _startSeq, ok, resp)) {
      const char* okp = _defs[_idx].okPrefix ? _defs[_idx].okPrefix : "OK";
      if (resp && strncmp(resp, okp, strlen(okp)) == 0) {
        _dirty[_idx] = false;
        _idx = nextSaveIdx(_idx + 1);
        if (_idx < 0) _phase = Phase::Ready; else fireSet(nowMs);
      } else {
        _errIdx = _idx; _phase = Phase::Error;
      }
    } else {
      _req.rearm(seqNow);
    }
  } else if (st == PendingRequest::State::TimedOut) {
    _errIdx = _idx; _phase = Phase::Error;
  }
}

}  // namespace mishmesh
