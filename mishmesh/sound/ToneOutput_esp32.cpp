#if defined(ESP32_PLATFORM) && defined(PIN_BUZZER)
#include <mishmesh/sound/ToneOutput.h>
#include <Arduino.h>

namespace mishmesh { namespace sound {

// ESP32 LEDC: ledcWriteTone sets pitch (at fixed 50% duty); ledcWrite then
// overrides the duty count for volume. 10-bit resolution => full scale 1023.
class ToneOutputEsp32 : public IToneOutput {
public:
  void begin() override { ledcAttach(PIN_BUZZER, 1000, 10); silence(); }
  void tone(uint16_t freqHz, VolumeLevel vol) override {
    if (vol == VolumeLevel::Mute || freqHz == 0) { silence(); return; }
    ledcWriteTone(PIN_BUZZER, freqHz);
    uint32_t duty;
    switch (vol) {
      case VolumeLevel::High: duty = 512; break;   // ~50% of 1024
      case VolumeLevel::Mid:  duty = 128; break;
      default:                duty = 32;  break;    // Low
    }
    ledcWrite(PIN_BUZZER, duty);
  }
  void silence() override { ledcWrite(PIN_BUZZER, 0); }
};

static ToneOutputEsp32 s_out;
IToneOutput* defaultToneOutput() { return &s_out; }

}}  // namespace mishmesh::sound
#endif
