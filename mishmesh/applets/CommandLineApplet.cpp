// mishmesh/applets/CommandLineApplet.cpp
#include <mishmesh/applets/CommandLineApplet.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

void CommandLineApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void CommandLineApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
  _logCount = 0;
  _pending = false;
  _req.reset();
  _req.timeoutMs = 20000;   // LoRa round-trip can be slow (multi-hop / duty-cycled); login uses 25s
  _view.setWrap(true);      // CLI replies can be long; wrap them instead of ellipsizing
  syncView();
}

// Embedded-mode activation: updates context references. The log and pending state
// are preserved across tab switches (fresh only on hub open, via the hub calling onStart).
void CommandLineApplet::onShow(AppletContext& ctx) {
  _host = ctx.host;
  _svc = ctx.contacts;
}

void CommandLineApplet::pushLine(const char* s) {
  if (_logCount >= LOG_LINES) {           // full: drop the oldest line
    for (int i = 1; i < LOG_LINES; i++) memcpy(_log[i - 1], _log[i], LOG_COLS);
    _logCount = LOG_LINES - 1;
  }
  strncpy(_log[_logCount], s ? s : "", LOG_COLS - 1);
  _log[_logCount][LOG_COLS - 1] = 0;
  _logCount++;
}

void CommandLineApplet::appendMultiline(const char* s) {
  if (!s || !s[0]) { pushLine(""); return; }
  char seg[LOG_COLS];
  int n = 0;
  for (const char* p = s; ; p++) {
    if (*p == '\n' || *p == 0) {
      seg[n < LOG_COLS ? n : LOG_COLS - 1] = 0;
      pushLine(seg);
      n = 0;
      if (*p == 0) break;
    } else if (n < LOG_COLS - 1) {
      seg[n++] = *p;
    }
  }
}

void CommandLineApplet::syncView() {
  _view.clear(true);
  for (int i = 0; i < _logCount; i++) _view.addLine(_log[i]);
  _view.scrollToEnd();
}

void CommandLineApplet::onCmdDone(void* ctx, const char* text) {
  auto* self = static_cast<CommandLineApplet*>(ctx);
  if (text && text != self->_cmdBuf) {
    copyStr(self->_cmdBuf, sizeof(self->_cmdBuf), text);
  }
  self->submitCommand(self->_cmdBuf);
}

void CommandLineApplet::submitCommand(const char* cmd) {
  if (!cmd || !cmd[0]) return;
  char echo[LOG_COLS];
  snprintf(echo, sizeof(echo), "> %s", cmd);
  pushLine(echo);
  if (_svc) {
    _cliStartSeq = _svc->cliSeq();
    if (_svc->sendCliCommand(_pub, cmd)) {
      _req.begin(_cliStartSeq, _host ? _host->nowMs() : 0);
      _pending = true;
    } else {
      pushLine("[send failed]");
    }
  } else {
    pushLine("[send failed]");
  }
  syncView();
  // State changed outside onRender: force the next loop to render (start polling
  // now rather than waiting out the previous render's returned delay).
  if (_host) _host->requestRender();
}

int CommandLineApplet::onRender(Canvas& c) {
  return renderBody(c, 0, 0, c.width(), c.height());
}

// Embedded-mode render: draws the CLI log and hint into a sub-region below the TabBar.
int CommandLineApplet::renderBody(Canvas& c, int x, int y, int w, int h) {
  const Font* cap = fontCaption();
  int caph = c.lineHeight(cap);

  if (_pending && _svc) {
    PendingRequest::State s = _req.poll(_svc->cliSeq(), c.now());
    if (s == PendingRequest::State::Ready) {
      bool ok = false; const char* resp = nullptr;
      if (_svc->cliResult(_pub, _cliStartSeq, ok, resp)) {
        appendMultiline(resp);
        _pending = false;
        syncView();
      } else {
        _req.rearm(_svc->cliSeq());
      }
    } else if (s == PendingRequest::State::TimedOut) {
      pushLine("[no response]");
      _pending = false;
      syncView();
    }
  }

  const char* hint = _pending ? "Waiting reply..." : "Select: command";
  int bottom = y + h - caph - 1;
  _view.draw(c, x, y, w, bottom - y);
  c.drawText(cap, x + 2, bottom, hint, DisplayDriver::LIGHT, TextAlign::Left);

  if (_pending) return 120;
  if (_view.needsAnimation()) return 33;
  return 1000;
}

bool CommandLineApplet::onInput(InputEvent ev) {
  if (_view.onInput(ev)) return true;        // NavUp/Down scroll the log
  if (ev == InputEvent::Select) {
    if (_pending) {                         // one in-flight request at a time
      if (_host) _host->postToast("Waiting reply...");
      return true;
    }
    _cmdBuf[0] = 0;
    keypadApplet().configure(_cmdBuf, sizeof(_cmdBuf) - 1, "Command",
                             &CommandLineApplet::onCmdDone, this);
    if (_host) _host->push(&keypadApplet());
    return true;
  }
  return false;   // Back bubbles: host pops us back to the hub
}

CommandLineApplet& commandLineApplet() {
  static CommandLineApplet s_commandLine;
  return s_commandLine;
}

}  // namespace mishmesh
