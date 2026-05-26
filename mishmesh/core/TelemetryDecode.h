#pragma once

#include <stdint.h>
#include <mishmesh/core/ContactsService.h>

namespace mishmesh {

// Decode a raw CayenneLPP buffer into a fixed-size field list (reuses LPPReader).
void decodeTelemetry(const uint8_t* lpp, uint8_t len, TelemetryView& out);

// Short human name for an LPP type ("Voltage", "Temp", "Humidity", ...).
const char* telemTypeName(uint8_t lppType);

// Format a field's value + unit into buf (e.g. "3.92V", "21.4C", "57%", GPS "lat,lon").
void formatField(const TelemetryField& f, char* buf, int bufSize);

}  // namespace mishmesh
