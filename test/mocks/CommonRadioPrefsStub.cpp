// Host stand-in for src/helpers/CommonRadioPrefs.cpp, which includes target.h and
// drives radio_driver. NodePrefs needs its out-of-line members to link; none of the
// host tests go through the radio CLI or the fem_* keys, so they all decline.
#include <helpers/CommonRadioPrefs.h>

bool CommonRadioPrefs::handleCommand(const char* command, uint32_t sender_timestamp, char* reply) {
  return false;
}

bool CommonRadioPrefs::setByKey(const char* key, const char* value) {
  return false;
}

bool CommonRadioPrefs::getByKey(const char* key, char* value, size_t max_len) {
  return false;
}
