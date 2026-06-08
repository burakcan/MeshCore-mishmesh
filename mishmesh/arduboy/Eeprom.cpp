#include <mishmesh/arduboy/Eeprom.h>
#include <mishmesh/core/AppletStorage.h>

namespace mishmesh { namespace arduboy {

void EepromImage::load(AppletStorage* s, const char* key) {
  if (s == nullptr) return;
  uint8_t n = s->load(key, _buf, EEPROM_IMAGE_BYTES);
  for (int i = n; i < EEPROM_IMAGE_BYTES; i++) _buf[i] = 0;
  _dirty = false;
}

void EepromImage::saveIfDirty(AppletStorage* s, const char* key) {
  if (!_dirty) return;
  if (s != nullptr && s->save(key, _buf, EEPROM_IMAGE_BYTES)) _dirty = false;
}

}}  // namespace mishmesh::arduboy
