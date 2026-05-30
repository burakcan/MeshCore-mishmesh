#pragma once

namespace mishmesh {

// Integer ease-out toward a target, one call per frame: cover half the remaining
// distance but at least minStep px so the tail doesn't crawl. Used by the list
// and text-scroll widgets so movement glides at a consistent feel.
inline int approach(int cur, int tgt, int minStep) {
  int d = tgt - cur;
  if (d == 0) return cur;
  int ad = d < 0 ? -d : d;
  int step = ad / 2;
  if (step < minStep) step = minStep;
  if (step > ad) step = ad;
  return cur + (d > 0 ? step : -step);
}

}  // namespace mishmesh
