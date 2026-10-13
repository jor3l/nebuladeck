#pragma once
#include "display.h"

// Slow warped-noise nebula, rendered at 1/4 resolution and zoomed to the frame.
class Nebula {
 public:
  static constexpr int W = 80, H = 120, ZOOM = 4;
  explicit Nebula(LovyanGFX* parent) : buf_(parent) {}
  bool begin();
  // Renders the nebula (t in seconds) and stars into dst (320x480).
  void draw(LovyanGFX& dst, float t);

 private:
  lgfx::LGFX_Sprite buf_;
  uint16_t lut_[256];
};
