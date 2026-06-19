#pragma once

#include <stdint.h>

namespace mishmesh {

struct AppletStorage;

// Up to MAX_ITEMS canned messages, held in a fixed RAM array and (once begin()'d)
// persisted one blob per reply via AppletStorage. Shared singleton so the thread
// picker and the Settings panel see one list. No heap; all storage is inline.
class QuickReplyStore {
public:
  static const int     MAX_ITEMS = 10;
  static const uint8_t MAX_LEN   = 64;   // chars per reply (buffer is MAX_LEN + 1)

  int count() const { return _count; }
  const char* text(int i) const { return (i >= 0 && i < _count) ? _items[i] : ""; }

  bool add(const char* s);          // append; false if full or blank
  bool set(int i, const char* s);   // overwrite; false on bad index or blank
  bool remove(int i);               // delete + shift down; false on bad index
  // Reorder IN MEMORY only - does NOT persist. Grab-and-move calls this once per
  // step, and each persist() is a full 10-blob flash rewrite, so batching the save
  // to the drop keeps a multi-step drag fast. Call save() to commit.
  bool move(int from, int to);      // false on bad index or no-op
  void save();                      // persist the current order (commit batched moves)

  void begin(AppletStorage* storage);   // load persisted replies (no seeding)

private:
  void persist();                        // rewrite qr0..qr9 to storage

  char           _items[MAX_ITEMS][MAX_LEN + 1] = {};
  int            _count = 0;
  AppletStorage* _storage = nullptr;
};

QuickReplyStore& quickReplyStore();

}  // namespace mishmesh
