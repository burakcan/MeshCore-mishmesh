#pragma once

#include <stdint.h>
#include <mishmesh/core/Applet.h>   // RadioConfig

namespace mishmesh {

enum class RadioField : uint8_t { Frequency, Bandwidth, SF, CR, TxPower };

// Sink for radio sub-pickers to write staged edits back into the panel, without
// coupling the pickers to the RadioSettingsPanel singleton.
class RadioStagingTarget {
public:
  virtual ~RadioStagingTarget() {}
  virtual const RadioConfig& stagedConfig() const = 0;
  virtual void stageFrequency(float mhz) = 0;    // clamps
  virtual void stageBandwidth(float khz) = 0;
  virtual void stageSf(uint8_t sf) = 0;
  virtual void stageCr(uint8_t cr) = 0;
  virtual void stageTxPower(int dbm) = 0;        // clamps
  virtual void stagePreset(int presetIndex) = 0;
};

}  // namespace mishmesh
