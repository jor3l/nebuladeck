#pragma once
#include "display.h"

// Two tileable nebula layers, baked once at full resolution and scrolled against each
// other every frame (two byte reads + a palette lookup per pixel). Ripples only touch
// the pixels inside their ring.
class Nebula {
 public:
  static constexpr int W = 320, H = 480;
  bool begin();
  // Renders the nebula (t in seconds) and stars into dst (a 16-bit 320x480 sprite).
  void draw(lgfx::LGFX_Sprite& dst, float t);
  // Starts a ripple at screen coordinates (x, y) at time t (seconds).
  void ripple(int x, int y, float t);

 private:
  struct Ripple { float x, y, t0; bool on; };
  static constexpr int MAXRIP = 5;
  static constexpr float LIFE = 1.6f;
  Ripple rip_[MAXRIP] = {};
  int ripNext_ = 0;
  uint8_t *a_ = nullptr, *b_ = nullptr;  // PSRAM, W*H each
  uint16_t lut_[256];                    // byte-swapped RGB565, as the sprite stores it
  int16_t boost_[W] = {};
};
