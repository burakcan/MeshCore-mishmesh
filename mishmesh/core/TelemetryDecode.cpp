#include <mishmesh/core/TelemetryDecode.h>
#include <helpers/sensors/LPPDataHelpers.h>
#include <stdio.h>

namespace mishmesh {

void decodeTelemetry(const uint8_t* lpp, uint8_t len, TelemetryView& out) {
  out.valid = false;
  out.count = 0;
  if (!lpp || len == 0) return;

  LPPReader reader(lpp, len);
  uint8_t channel, type;
  while (out.count < MAX_TELEM_FIELDS && reader.readHeader(channel, type)) {
    TelemetryField& f = out.fields[out.count];
    f.channel = channel; f.type = type; f.value = f.value2 = f.value3 = 0;
    bool ok = true;
    switch (type) {
      case LPP_VOLTAGE:             ok = reader.readVoltage(f.value); break;
      case LPP_TEMPERATURE:         ok = reader.readTemperature(f.value); break;
      case LPP_RELATIVE_HUMIDITY:   ok = reader.readRelativeHumidity(f.value); break;
      case LPP_BAROMETRIC_PRESSURE: ok = reader.readPressure(f.value); break;
      case LPP_CURRENT:             ok = reader.readCurrent(f.value); break;
      case LPP_POWER:               ok = reader.readPower(f.value); break;
      case LPP_ALTITUDE:            ok = reader.readAltitude(f.value); break;
      case LPP_GPS:                 ok = reader.readGPS(f.value, f.value2, f.value3); break;
      default: reader.skipData(type); ok = false; break;   // recognised structurally, not surfaced
    }
    if (ok) out.count++;
  }
  out.valid = true;
}

const char* telemTypeName(uint8_t t) {
  switch (t) {
    case LPP_VOLTAGE:             return "Voltage";
    case LPP_TEMPERATURE:         return "Temp";
    case LPP_RELATIVE_HUMIDITY:   return "Humidity";
    case LPP_BAROMETRIC_PRESSURE: return "Pressure";
    case LPP_CURRENT:             return "Current";
    case LPP_POWER:               return "Power";
    case LPP_ALTITUDE:            return "Altitude";
    case LPP_GPS:                 return "GPS";
    default:                      return "Field";
  }
}

void formatField(const TelemetryField& f, char* buf, int bufSize) {
  switch (f.type) {
    case LPP_VOLTAGE:             snprintf(buf, bufSize, "%.2fV", f.value); break;
    case LPP_TEMPERATURE:         snprintf(buf, bufSize, "%.1fC", f.value); break;
    case LPP_RELATIVE_HUMIDITY:   snprintf(buf, bufSize, "%.0f%%", f.value); break;
    case LPP_BAROMETRIC_PRESSURE: snprintf(buf, bufSize, "%.0fhPa", f.value); break;
    case LPP_CURRENT:             snprintf(buf, bufSize, "%.3fA", f.value); break;
    case LPP_POWER:               snprintf(buf, bufSize, "%.0fW", f.value); break;
    case LPP_ALTITUDE:            snprintf(buf, bufSize, "%.0fm", f.value); break;
    case LPP_GPS:                 snprintf(buf, bufSize, "%.4f,%.4f", f.value, f.value2); break;
    default:                      snprintf(buf, bufSize, "%.2f", f.value); break;
  }
}

}  // namespace mishmesh
