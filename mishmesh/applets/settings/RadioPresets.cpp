// mishmesh/applets/settings/RadioPresets.cpp
// Single definition of the community radio-preset table. Declared extern in the header so
// the ~830B table is emitted once, not once per including TU (RepeaterRadioPanel,
// RadioPresetPickerApplet, RadioSettingsPanel) - the BLE firmware build is flash-tight.
#include <mishmesh/applets/settings/RadioPresets.h>

namespace mishmesh {

// Deprecated entries dropped; "USA/Canada" carries no "(Recommended)" suffix.
const RadioPreset PRESETS[] = {
  { "Australia",               915.800f, 10, 250.0f, 5 },
  { "Australia (Narrow)",      916.575f,  7,  62.5f, 8 },
  { "Australia (Mid)",         915.075f,  9, 125.0f, 5 },
  { "Australia: SA, WA",       923.125f,  8,  62.5f, 8 },
  { "Australia: QLD",          923.125f,  8,  62.5f, 5 },
  { "Brazil",                  923.125f,  8,  62.5f, 8 },
  { "EU/UK (Narrow)",          869.618f,  8,  62.5f, 8 },
  { "Czech Republic (Narrow)", 869.432f,  7,  62.5f, 5 },
  { "EU 433MHz (Long Range)",  433.650f,  8,  62.5f, 8 },
  { "Netherlands",             869.618f,  7,  62.5f, 5 },
  { "New Zealand",             917.375f, 11, 250.0f, 5 },
  { "New Zealand (Narrow)",    917.375f,  7,  62.5f, 5 },
  { "Portugal 433",            433.375f,  9,  62.5f, 6 },
  { "Portugal 868",            869.618f,  7,  62.5f, 6 },
  { "Switzerland",             869.618f,  8,  62.5f, 8 },
  { "USA/Canada",              910.525f,  7,  62.5f, 5 },
  { "Vietnam (Narrow)",        920.250f,  8,  62.5f, 5 },
};
const int PRESET_COUNT = (int)(sizeof(PRESETS) / sizeof(PRESETS[0]));

}  // namespace mishmesh
