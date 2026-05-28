#pragma once

#include <stdint.h>
#include <math.h>

namespace mishmesh {

// Great-circle distance in km between two points given as degrees * 1e6 (the
// int32 form MeshCore stores GPS coordinates in). Pure math, host-testable.
inline float geoDistanceKm(int32_t lat1e6, int32_t lon1e6, int32_t lat2e6, int32_t lon2e6) {
  const double R = 6371.0;            // mean earth radius, km
  const double D2R = 3.14159265358979323846 / 180.0;
  double lat1 = (lat1e6 / 1e6) * D2R;
  double lat2 = (lat2e6 / 1e6) * D2R;
  double dlat = ((lat2e6 - lat1e6) / 1e6) * D2R;
  double dlon = ((lon2e6 - lon1e6) / 1e6) * D2R;
  double a = sin(dlat / 2) * sin(dlat / 2) +
             cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
  return (float)(2.0 * R * asin(sqrt(a < 0 ? 0 : (a > 1 ? 1 : a))));
}

}  // namespace mishmesh
