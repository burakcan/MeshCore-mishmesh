#if defined(NRF52_PLATFORM) && defined(PIN_BUZZER)
#include <mishmesh/sound/ToneOutput.h>
#include <Arduino.h>

namespace mishmesh { namespace sound {

// nRF52 PWM: 16MHz / DIV_128 = 125kHz timer. countertop = 125000/freq sets the
// period (pitch); the compare value (duty) sets loudness on the passive piezo.
class ToneOutputNrf52 : public IToneOutput {
public:
  void begin() override {
    if (_pwm) return;
    _pwm = &HwPWM0;
    _pwm->addPin(PIN_BUZZER);
    _pwm->setClockDiv(PWM_PRESCALER_PRESCALER_DIV_128);   // 125kHz
    silence();
  }
  void tone(uint16_t freqHz, VolumeLevel vol) override {
    if (!_pwm || vol == VolumeLevel::Mute || freqHz == 0) { silence(); return; }
    uint32_t top = 125000UL / freqHz;
    if (top < 4) top = 4;
    if (top > 32767) top = 32767;
    uint32_t duty;
    // Loudness is NOT monotonic in duty on this passive piezo: it peaks around a narrow
    // ~12% pulse (buzzer resonance) and falls off on BOTH sides - pulses wider toward 50%
    // get quieter, not louder. So keep the ramp on the rising (low-duty) side: High sits
    // at the ~12% peak, Mid/Low step down from there (Low ~3% is already properly quiet).
    switch (vol) {
      case VolumeLevel::High: duty = top / 8;  break;   // ~12% - resonance peak, loudest
      case VolumeLevel::Mid:  duty = top / 16; break;   // ~6%
      default:                duty = top / 32; break;   // Low ~3% - quietest
    }
    if (duty == 0) duty = 1;
    _pwm->setMaxValue(top);
    _pwm->writePin(PIN_BUZZER, (uint16_t)duty, false);
  }
  void silence() override {
    if (_pwm) _pwm->writePin(PIN_BUZZER, 0, false);
  }
private:
  HardwarePWM* _pwm = nullptr;
};

static ToneOutputNrf52 s_out;
IToneOutput* defaultToneOutput() { return &s_out; }

}}  // namespace mishmesh::sound
#endif
