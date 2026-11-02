#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mishmesh {

struct RadioConfig;
struct TxPaTable;

// Format one radio-config field into buf: idx 1=freq "%g MHz", 2=bw "%g kHz",
// 3=sf "SFn", 4=cr "CRn", 5=tx "n dBm". Shared by the radio settings panel and the
// repeater radio panel, which formatted these identically. Other idx values clear
// buf (preset/repeater rows are caller-specific). Returns buf.
const char* formatRadioField(char* buf, size_t cap, int idx, const RadioConfig& cfg);

// Antenna-port power in tenths of a dBm for a chip setting; false when pa is null
// or has no measurement for it.
bool txPaOutputDeciDbm(const TxPaTable* pa, int setting, int16_t& out);

// TX power row text. No PA: "n dBm". With a PA: "n -> ~30 dBm" (chip setting,
// then measured output), or "n dBm chip" for settings the table does not cover.
const char* formatTxPower(char* buf, size_t cap, int setting, const TxPaTable* pa);

}  // namespace mishmesh
