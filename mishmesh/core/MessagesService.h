// mishmesh/core/MessagesService.h
#pragma once
#include <stdint.h>
#include <mishmesh/core/MessageStore.h>   // ConvoKey

namespace mishmesh {

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
  int8_t      snrx4;
  uint8_t     hops;
  const uint8_t* path; uint8_t pathLen;
};
struct RepeatView {
  const char* repeaterName;  // resolved or hex; "" if unknown
  uint8_t     knownCount;    // # contacts matching the 1-byte hash (collisions)
  uint8_t     hops;
  int8_t      snrx4;
};

struct MessagesService {
  virtual ~MessagesService() {}
  virtual int  convoCount() const = 0;
  virtual bool getConvo(int i, ConvoView& out) const = 0;
  virtual uint16_t totalUnread() const = 0;
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
  // v1 no-ops (no on-device input)
  virtual void newMessage() {}
  virtual void newGroup() {}
  virtual void joinChannel() {}
  virtual void reply(const ConvoKey&, int) {}
  virtual uint32_t seq() const = 0;
};

} // namespace mishmesh
