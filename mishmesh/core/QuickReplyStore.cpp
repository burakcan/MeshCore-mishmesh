#include <mishmesh/core/QuickReplyStore.h>
#include <mishmesh/core/AppletStorage.h>
#include <string.h>
#include <stdio.h>

namespace mishmesh {

// Out-of-line definitions required when static const int members are ODR-used
// (e.g. passed to EXPECT_EQ by const-ref in tests). Redundant in C++17 but
// keeps the build clean on all standards.
const int     QuickReplyStore::MAX_ITEMS;
const uint8_t QuickReplyStore::MAX_LEN;

// Copy src into dst, trimming to MAX_LEN chars. dst is MAX_LEN+1 bytes.
static int qrCopy(char* dst, const char* src) {
  int n = 0;
  if (src) for (; src[n] && n < QuickReplyStore::MAX_LEN; n++) dst[n] = src[n];
  dst[n] = 0;
  return n;
}

bool QuickReplyStore::add(const char* s) {
  if (!s || !s[0] || _count >= MAX_ITEMS) return false;
  if (qrCopy(_items[_count], s) == 0) return false;
  _count++;
  persist();
  return true;
}

bool QuickReplyStore::set(int i, const char* s) {
  if (i < 0 || i >= _count || !s || !s[0]) return false;
  char tmp[MAX_LEN + 1];
  if (qrCopy(tmp, s) == 0) return false;
  memcpy(_items[i], tmp, sizeof(tmp));
  persist();
  return true;
}

bool QuickReplyStore::remove(int i) {
  if (i < 0 || i >= _count) return false;
  for (int j = i; j < _count - 1; j++) memcpy(_items[j], _items[j + 1], sizeof(_items[j]));
  _count--;
  _items[_count][0] = 0;
  persist();
  return true;
}

bool QuickReplyStore::move(int from, int to) {
  if (from < 0 || from >= _count || to < 0 || to >= _count || from == to) return false;
  char tmp[MAX_LEN + 1];
  memcpy(tmp, _items[from], sizeof(tmp));
  if (to > from) for (int j = from; j < to; j++) memcpy(_items[j], _items[j + 1], sizeof(_items[j]));
  else           for (int j = from; j > to; j--) memcpy(_items[j], _items[j - 1], sizeof(_items[j]));
  memcpy(_items[to], tmp, sizeof(tmp));
  return true;   // in-memory only; caller commits with save()
}

void QuickReplyStore::save() { persist(); }

void QuickReplyStore::begin(AppletStorage* storage) {
  _storage = storage;
  _count = 0;
  for (int i = 0; i < MAX_ITEMS; i++) _items[i][0] = 0;
  if (!storage) return;
  for (int i = 0; i < MAX_ITEMS; i++) {
    char key[8];
    snprintf(key, sizeof(key), "qr%d", i);
    uint8_t got = storage->load(key, (uint8_t*)_items[i], MAX_LEN);
    _items[i][got] = 0;
    if (_items[i][0] == 0) break;   // first empty/cleared key ends the contiguous list
    _count = i + 1;
  }
}

void QuickReplyStore::persist() {
  if (!_storage) return;
  for (int i = 0; i < MAX_ITEMS; i++) {
    char key[8];
    snprintf(key, sizeof(key), "qr%d", i);
    if (i < _count && _items[i][0]) {
      _storage->save(key, (const uint8_t*)_items[i], (uint8_t)strlen(_items[i]));
    } else {
      uint8_t zero = 0;
      _storage->save(key, &zero, 1);   // cleared marker (reads back as length 0)
    }
  }
}

QuickReplyStore& quickReplyStore() {
  static QuickReplyStore s;
  return s;
}

}  // namespace mishmesh
