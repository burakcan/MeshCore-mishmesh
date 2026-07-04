#include <mishmesh/core/CliRequest.h>
#include <mishmesh/core/ContactsService.h>

namespace mishmesh {

uint32_t cliFire(ContactsService* svc, PendingRequest& req, const uint8_t* pub,
                 const char* cmd, uint32_t nowMs, uint32_t timeoutMs) {
  uint32_t seq = svc ? svc->cliSeq() : 0;
  req.timeoutMs = timeoutMs;
  req.begin(seq, nowMs);
  if (svc) svc->sendCliCommand(pub, cmd);
  return seq;
}

CliPoll cliPoll(ContactsService* svc, PendingRequest& req, const uint8_t* pub,
                uint32_t startSeq, uint32_t nowMs, bool& ok, const char*& resp) {
  if (!svc) return CliPoll::Pending;
  PendingRequest::State st = req.poll(svc->cliSeq(), nowMs);
  if (st == PendingRequest::State::Ready) {
    if (svc->cliResult(pub, startSeq, ok, resp)) return CliPoll::Ready;
    req.rearm(svc->cliSeq());
    return CliPoll::Pending;
  }
  if (st == PendingRequest::State::TimedOut) return CliPoll::TimedOut;
  return CliPoll::Pending;
}

}  // namespace mishmesh
