#include "nebula.h"
#include <math.h>

static inline float hash2(int x, int y) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return ((h ^ (h >> 16)) & 0xFFFF) * (1.0f / 65535.0f);
}

static inline float vnoise(float x, float y) {
  int xi = (int)floorf(x), yi = (int)floorf(y);
  float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3.0f - 2.0f * fx);
  fy = fy * fy * (3.0f - 2.0f * fy);
  float a = hash2(xi, yi), b = hash2(xi + 1, yi);
  float c = hash2(xi, yi + 1), d = hash2(xi + 1, yi + 1);
  return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

static inline float fbm2(float x, float y) {
  return 0.6f * vnoise(x, y) + 0.4f * vnoise(x * 2.03f + 17.0f, y * 2.03f + 5.0f);
}

static inline float fbm3(float x, float y) {
  return 0.5f * vnoise(x, y) + 0.3f * vnoise(x * 2.03f + 17.0f, y * 2.03f + 5.0f) +
         0.2f * vnoise(x * 4.1f + 3.0f, y * 4.1f + 11.0f);
}

bool Nebula::begin() {
  buf_.setColorDepth(16);
  if (!buf_.createSprite(W, H)) return false;
  // Deep space -> violet -> magenta -> cyan highlights.
  static const struct { float at; uint8_t r, g, b; } stops[] = {
      {0.00f, 2, 3, 14},    {0.30f, 10, 8, 40},   {0.50f, 38, 14, 86},
      {0.70f, 112, 30, 140}, {0.86f, 226, 64, 160}, {1.00f, 120, 220, 255}};
  for (int i = 0; i < 256; i++) {
    float v = i / 255.0f;
    int k = 0;
    while (k < 4 && v > stops[k + 1].at) k++;
    float f = (v - stops[k].at) / (stops[k + 1].at - stops[k].at);
    auto mix = [&](uint8_t a, uint8_t b) { return (int)(a + (b - a) * f); };
    lut_[i] = buf_.color565(mix(stops[k].r, stops[k + 1].r), mix(stops[k].g, stops[k + 1].g),
                            mix(stops[k].b, stops[k + 1].b));
  }
  return true;
}

void Nebula::draw(LovyanGFX& dst, float t) {
  const float s = 0.045f;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      float px = x * s, py = y * s;
      float q = fbm2(px + t * 0.05f, py - t * 0.03f);
      float v = fbm3(px + 3.2f * q - t * 0.04f, py + 3.2f * q + t * 0.02f);
      v = (v - 0.18f) * 1.55f;
      int i = (int)(v * 255.0f);
      buf_.drawPixel(x, y, lut_[i < 0 ? 0 : i > 255 ? 255 : i]);
    }
  }
  buf_.pushRotateZoomWithAA(&dst, dst.width() / 2.0f, dst.height() / 2.0f, 0, ZOOM, ZOOM);

  // Stars: fixed positions, slow twinkle.
  for (int i = 0; i < 46; i++) {
    int sx = (int)(hash2(i, 91) * dst.width());
    int sy = (int)(hash2(i, 37) * dst.height());
    float tw = 0.5f + 0.5f * sinf(t * (0.6f + hash2(i, 5) * 1.6f) + i);
    int b = 70 + (int)(tw * 150);
    dst.drawPixel(sx, sy, dst.color565(b, b, b + 20 > 255 ? 255 : b + 20));
    if (i % 7 == 0) {
      int d = b / 3;
      uint16_t c = dst.color565(d, d, d + 10);
      dst.drawPixel(sx - 1, sy, c); dst.drawPixel(sx + 1, sy, c);
      dst.drawPixel(sx, sy - 1, c); dst.drawPixel(sx, sy + 1, c);
    }
  }
}
