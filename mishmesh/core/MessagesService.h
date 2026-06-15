// mishmesh/core/MessagesService.h
#pragma once
#include <stdint.h>
#include <mishmesh/core/MessageStore.h>   // ConvoKey

namespace mishmesh {

// Result of an on-device channel create/join. Drives the applet's toast.
enum class ChanResult : int8_t { Ok, Full, Invalid, Duplicate, Error };

// Per-chat notification level. All == 0 so an absent/default setting reads as All.
enum class NotifyLevel : uint8_t { All = 0, MentionsOnly = 1, Mute = 2 };

// Short label for the ChatMenu "Notifications" row value column.
inline const char* notifyLevelShortLabel(NotifyLevel lvl) {
  switch (lvl) {
    case NotifyLevel::Mute:         return "Mute";
    case NotifyLevel::MentionsOnly: return "Mentions";
    default:                        return "All";
  }
}

// Global messages-app settings (the Messages "Settings" tab). autoRetry and
// autoResetPath have no firmware mechanism yet, so the adapter only persists
// them (stored placeholders). directAcks reuses NodePrefs.multi_acks: 1 = a
// single ack on receive, 2 = the redundant double-ack.
struct MessagesConfig {
  bool    autoRetry     = false;
  bool    autoResetPath = false;
  uint8_t directAcks    = 1;   // 1 or 2
};

struct ConvoView {
  ConvoKey    key;
  bool        isChannel;
  const char* name;        // resolved; valid until next call
  uint32_t    lastTime;
  uint16_t    unread;
  const char* preview;     // last message body, truncated
};
struct MessageView {
  bool        outbound;
  bool        isChannel;
  uint8_t     kind;
  uint32_t    senderTime, localTime;
  const char* senderName;  // "" when not applicable
  const char* text;        // null-terminated, group "name: " stripped
  uint8_t     status;      // MsgStatus
  uint16_t    tripTimeMs;
  uint8_t     heardCount;
  uint8_t     retryAttempt; // outbound DM still pending: current auto-retry number (0 = none)
  int8_t      snrx4;
  uint8_t     hops;
  const uint8_t* path; uint8_t pathLen;
};
struct RepeatView {
  uint8_t        hops;
  int8_t         snrx4;
  const uint8_t* path;     // hop hash-bytes; resolve via resolveHop()
  uint8_t        pathLen;
};

struct MessagesService {
  virtual ~MessagesService() {}
  virtual int  convoCount() const = 0;
  virtual bool getConvo(int i, ConvoView& out) const = 0;
  virtual uint16_t totalUnread() const = 0;
  virtual uint16_t totalNotifyUnread() const { return 0; }
  virtual int  messageCount(const ConvoKey& k) const = 0;
  virtual bool getMessage(const ConvoKey& k, int i, MessageView& out) const = 0;
  virtual void setActiveConvo(const ConvoKey& k) = 0;
  virtual void clearActiveConvo() = 0;
  virtual int  repeatCount(const ConvoKey& k, int msgIdx) const = 0;
  virtual bool getRepeat(const ConvoKey& k, int msgIdx, int r, RepeatView& out) const = 0;
  virtual bool resolveHop(uint8_t hashByte, const char*& name, uint8_t& knownCount) const = 0;
  virtual void deleteMessage(const ConvoKey& k, int i) = 0;
  virtual void clearConvo(const ConvoKey& k) = 0;
  virtual void deleteConvo(const ConvoKey& k) = 0;
  virtual void markUnread(const ConvoKey& k) = 0;
  // On-device send. type==0 direct (id = 6-byte pubkey prefix), type==1 channel
  // (id[0] = channel index). Returns false on failure (table full / not found).
  virtual bool sendText(const ConvoKey& k, const char* text) = 0;
  // Per-chat region (MeshCore default flood scope, by name). Adapter-backed and
  // applied as a per-send scope override; default no-ops so non-adapter impls
  // (tests) stay inert. region() fills dst (NUL-terminated) and returns the name
  // length, 0 when unset ("None"). setRegion(name=="" / null) clears it.
  virtual int  region(const ConvoKey& k, char* dst, int cap) const {
    (void)k; if (dst && cap > 0) dst[0] = 0; return 0;
  }
  virtual void setRegion(const ConvoKey& k, const char* name) { (void)k; (void)name; }
  // Per-chat notification level. Adapter-backed; defaults keep non-adapter
  // impls (tests) inert at All.
  virtual NotifyLevel notifyLevel(const ConvoKey& k) const { (void)k; return NotifyLevel::All; }
  virtual void setNotifyLevel(const ConvoKey& k, NotifyLevel lvl) { (void)k; (void)lvl; }
  // Global Messages settings. Adapter-backed; defaults keep non-adapter impls
  // (tests) inert at the struct defaults.
  virtual MessagesConfig getMessagesConfig() const { return MessagesConfig(); }
  virtual void setMessagesConfig(const MessagesConfig& cfg) { (void)cfg; }
  // v1 no-ops (no on-device input)
  virtual void newMessage() {}
  virtual void newGroup() {}
  virtual void joinChannel() {}
  virtual void reply(const ConvoKey&, int) {}
  // On-device channel ops. Defaults make non-adapter implementations inert.
  // createPrivate: random 16-byte secret. joinPrivate: secret from 32-hex key.
  // joinPublic: fixed "Public" name + well-known PSK. joinHashtag: PSK from
  // sha256("#"+name) (name stored as "#name"); see companion_protocol.md.
  virtual ChanResult createPrivateChannel(const char* name) { (void)name; return ChanResult::Error; }
  virtual ChanResult joinPrivateChannel(const char* name, const char* keyHex) { (void)name; (void)keyHex; return ChanResult::Error; }
  virtual ChanResult joinPublicChannel() { return ChanResult::Error; }
  virtual ChanResult joinHashtagChannel(const char* hashtag) { (void)hashtag; return ChanResult::Error; }
  virtual bool publicChannelJoined() const { return false; }
  virtual uint32_t seq() const = 0;
};

} // namespace mishmesh
