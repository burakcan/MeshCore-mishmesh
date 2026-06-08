// mishmesh-authored. Defines the process-global EEPROM image, its accessor, and
// the Arduino-style `EEPROM` global object the vendored Arduboy2 uses. The
// runtime (Task 6) reaches the image via mishmesh::arduboy::eeprom() to
// load/save it through AppletStorage; it does NOT define it.

#include <mishmesh/arduboy/ArduboyEeprom.h>

namespace mishmesh { namespace arduboy {

static EepromImage g_eeprom;

EepromImage& eeprom() { return g_eeprom; }

}}  // namespace mishmesh::arduboy

// The single Arduino-EEPROM facade instance referenced by the vendored library.
EEPROMClass EEPROM;
