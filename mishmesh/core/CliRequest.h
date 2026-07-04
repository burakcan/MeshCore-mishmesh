#pragma once

#include <stdint.h>
#include <mishmesh/core/PendingRequest.h>

namespace mishmesh {

class ContactsService;

// Shared boilerplate for the repeater panels' "fire an admin CLI command at a
// logged-in server, then poll cliSeq() until the reply latches" flow. Every panel
// had copy-pasted the same fire/poll/rearm sequence into its onRender; these
// collapse it to one out-of-line copy. Panels keep their own PendingRequest /
// _startSeq / _pub members and their own reply handling - only the plumbing moves.

// Snapshot cliSeq(), arm req for timeoutMs, and send cmd to pub. Returns the
// snapshot seq; store it as the panel's _startSeq for cliPoll(). timeoutMs is
// explicit (no default) so each call preserves its panel's existing timeout.
uint32_t cliFire(ContactsService* svc, PendingRequest& req, const uint8_t* pub,
                 const char* cmd, uint32_t nowMs, uint32_t timeoutMs);

enum class CliPoll : uint8_t { Pending, Ready, TimedOut };

// One poll step against cliSeq(). On Ready, resolves cliResult() into ok/resp;
// when the seq bump was for a different target it re-arms and returns Pending,
// exactly as the hand-written loops did.
CliPoll cliPoll(ContactsService* svc, PendingRequest& req, const uint8_t* pub,
                uint32_t startSeq, uint32_t nowMs, bool& ok, const char*& resp);

}  // namespace mishmesh
