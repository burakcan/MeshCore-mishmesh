// [mishmesh] Slide animation implementation. See Game2048Anim.h.
#include "Game2048Anim.h"
#include "platform.h"
// hex_digits is a file-scope `const` array (internal linkage in C++), so it is NOT
// exported from draw.cpp — include the header here to get our own private copy
// rather than an unresolved extern.
#include "hex-digits.h"

static const int8_t SLIDE_FRAMES = 3;   // ~100ms at 30fps; tune for feel

static Slide2048 s_slides[DIM * DIM];
static uint8_t   s_count;
static int8_t    s_frame;                // >0 while animating; counts down to 0

static inline int cellX(uint8_t a) { return BOARD_X + a * TILE_SZ; }
static inline int cellY(uint8_t b) { return BOARD_Y + b * TILE_SZ; }

uint8_t anim2048ComputeSlides(uint8_t direction, uint16_t board[DIM][DIM], Slide2048* out) {
  const bool horizontal = (direction == INPUT_LEFT || direction == INPUT_RIGHT);
  const bool wallAtZero = (direction == INPUT_LEFT || direction == INPUT_UP);
  uint8_t n = 0;

  for (uint8_t line = 0; line < DIM; line++) {
    int8_t  writePos = 0;      // next free slot along the line (wall-first)
    int8_t  lastWrite = -1;    // slot of the previous surviving tile
    uint16_t lastExp = 0;
    bool    lastMerged = false;

    for (uint8_t step = 0; step < DIM; step++) {
      // Walk the line wall-first so the collapse order matches the engine.
      uint8_t idx = wallAtZero ? step : (uint8_t)(DIM - 1 - step);
      uint8_t a = horizontal ? idx  : line;
      uint8_t b = horizontal ? line : idx;

      uint16_t v = board[a][b] & 0x7FFF;
      if (v == 0) continue;

      int8_t destPos;
      if (writePos > 0 && !lastMerged && v == lastExp) {
        destPos = lastWrite;        // merges into the previous tile's slot
        lastMerged = true;
      } else {
        destPos = writePos;
        lastWrite = writePos;
        lastExp = v;
        lastMerged = false;
        writePos++;
      }

      uint8_t destIdx = wallAtZero ? (uint8_t)destPos : (uint8_t)(DIM - 1 - destPos);
      out[n].fromA = a;
      out[n].fromB = b;
      out[n].toA = horizontal ? destIdx : line;
      out[n].toB = horizontal ? line    : destIdx;
      out[n].exp = v;
      n++;
    }
  }
  return n;
}

void anim2048Begin(uint8_t direction, uint16_t board[DIM][DIM]) {
  s_count = anim2048ComputeSlides(direction, board, s_slides);
  bool moves = false;
  for (uint8_t i = 0; i < s_count; i++) {
    if (s_slides[i].fromA != s_slides[i].toA || s_slides[i].fromB != s_slides[i].toB) {
      moves = true;
      break;
    }
  }
  s_frame = moves ? SLIDE_FRAMES : 0;
}

bool anim2048Active() { return s_frame > 0; }

void anim2048StepDraw() {
  if (s_frame <= 0) return;

  const int done = SLIDE_FRAMES - s_frame + 1;   // 1..SLIDE_FRAMES (fraction of the way)
  Platform::DrawFilledRect(BOARD_X, BOARD_Y, TILE_SZ * DIM, TILE_SZ * DIM, COLOUR_BLACK);

  for (uint8_t i = 0; i < s_count; i++) {
    const int fx = cellX(s_slides[i].fromA), fy = cellY(s_slides[i].fromB);
    const int tx = cellX(s_slides[i].toA),   ty = cellY(s_slides[i].toB);
    const int x = fx + (tx - fx) * done / SLIDE_FRAMES;
    const int y = fy + (ty - fy) * done / SLIDE_FRAMES;
    Platform::DrawBitmap(hex_digits[s_slides[i].exp], x, y, TILE_SZ, TILE_SZ, COLOUR_WHITE);
  }

  s_frame--;
}
