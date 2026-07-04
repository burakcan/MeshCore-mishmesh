#pragma once

#include <stddef.h>

namespace mishmesh {

struct RadioConfig;

// Format one radio-config field into buf: idx 1=freq "%g MHz", 2=bw "%g kHz",
// 3=sf "SFn", 4=cr "CRn", 5=tx "n dBm". Shared by the radio settings panel and the
// repeater radio panel, which formatted these identically. Other idx values clear
// buf (preset/repeater rows are caller-specific). Returns buf.
const char* formatRadioField(char* buf, size_t cap, int idx, const RadioConfig& cfg);

}  // namespace mishmesh
