#include "nebula.h"
#include <esp_heap_caps.h>
#include <math.h>

static constexpr int W = Nebula::W, H = Nebula::H, LW = W / 2, LH = H / 2;

static inline float hashp(int x, int y, uint32_t seed) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return ((h ^ (h >> 16)) & 0xFFFF) * (1.0f / 65535.0f);
}

// Value noise that repeats every px x py cells, so the layers tile when scrolled.
static inline float pnoise(float x, float y, int px, int py, uint32_t seed) {
  int xi = (int)floorf(x), yi = (int)floorf(y);
  float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3.0f - 2.0f * fx);
  fy = fy * fy * (3.0f - 2.0f * fy);
  int x0 = ((xi % px) + px) % px, y0 = ((yi % py) + py) % py;
  int x1 = (x0 + 1) % px, y1 = (y0 + 1) % py;
  float a = hashp(x0, y0, seed), b = hashp(x1, y0, seed);
  float c = hashp(x0, y1, seed), d = hashp(x1, y1, seed);
  return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

static float pfbm(float x, float y, int px, int py, int oct, uint32_t seed) {
  float sum = 0, amp = 0.5f, norm = 0;
  for (int o = 0; o < oct; o++) {
    sum += amp * pnoise(x, y, px, py, seed + o);
    norm += amp;
    x *= 2; y *= 2; px *= 2; py *= 2; amp *= 0.5f;
  }
  return sum / norm;
}

// Domain-warped fbm at half resolution, contrast-stretched, upsampled into out (W*H).
static bool bake(uint8_t* out, int px, int py, float warp, uint32_t seed) {
  uint8_t* lo = (uint8_t*)malloc(LW * LH);
  if (!lo) return false;
  int hist[256] = {};
  for (int y = 0; y < LH; y++)
    for (int x = 0; x < LW; x++) {
      float u = x * px / (float)LW, v = y * py / (float)LH;
      float q = pfbm(u, v, px, py, 2, seed + 100);
      float r = pfbm(u + 1.3f, v + 2.7f, px, py, 2, seed + 200);
      int i = (int)(pfbm(u + warp * q, v + warp * r, px, py, 4, seed) * 255.0f);
      i = i < 0 ? 0 : i > 255 ? 255 : i;
      lo[y * LW + x] = i;
      hist[i]++;
    }
  int n = 0, a = 0, b = 255;
  for (int i = 0; i < 256; i++) {
    n += hist[i];
    if (n < LW * LH / 100) a = i;
    if (n < LW * LH * 99 / 100) b = i;
  }
  uint8_t map[256];
  for (int i = 0; i < 256; i++) {
    int v = (i - a) * 255 / (b - a + 1);
    map[i] = v < 0 ? 0 : v > 255 ? 255 : v;
  }
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int x0 = x >> 1, y0 = y >> 1, x1 = (x0 + (x & 1)) % LW, y1 = (y0 + (y & 1)) % LH;
      int s = lo[y0 * LW + x0] + lo[y0 * LW + x1] + lo[y1 * LW + x0] + lo[y1 * LW + x1];
      out[y * W + x] = map[s >> 2];
    }
  free(lo);
  return true;
}

bool Nebula::begin() {
  a_ = (uint8_t*)heap_caps_malloc(W * H, MALLOC_CAP_SPIRAM);
  b_ = (uint8_t*)heap_caps_malloc(W * H, MALLOC_CAP_SPIRAM);
  if (!a_ || !b_ || !bake(a_, 3, 4, 1.6f, 1) || !bake(b_, 5, 7, 1.2f, 7)) return false;
  // Palettes, mid-tones stretched. Normal: deep space -> violet -> magenta -> cyan highlights.
  // Attention: the same shape in warm amber. Idle: the normal palette's luminance, cool grey.
  struct Stop { float at; uint8_t r, g, b; };
  static const Stop normal[] = {
      {0.00f, 2, 3, 14},    {0.30f, 10, 8, 40},   {0.50f, 38, 14, 86},
      {0.70f, 112, 30, 140}, {0.86f, 226, 64, 160}, {1.00f, 120, 220, 255}};
  static const Stop attn[] = {
      {0.00f, 6, 3, 6},     {0.30f, 34, 10, 12},  {0.50f, 92, 30, 14},
      {0.70f, 168, 72, 22},  {0.86f, 250, 150, 50}, {1.00f, 255, 232, 170}};
  auto build = [](const Stop* stops, uint8_t (*out)[3]) {
    for (int i = 0; i < 256; i++) {
      float v = (i / 255.0f - 0.26f) / (0.88f - 0.26f);
      v = v < 0 ? 0 : v > 1 ? 1 : v;
      int k = 0;
      while (k < 4 && v > stops[k + 1].at) k++;
      float f = (v - stops[k].at) / (stops[k + 1].at - stops[k].at);
      out[i][0] = stops[k].r + (stops[k + 1].r - stops[k].r) * f;
      out[i][1] = stops[k].g + (stops[k + 1].g - stops[k].g) * f;
      out[i][2] = stops[k].b + (stops[k + 1].b - stops[k].b) * f;
    }
  };
  build(normal, pal_[M_NORMAL]);
  build(attn, pal_[M_ATTN]);
  for (int i = 0; i < 256; i++) {
    const uint8_t* c = pal_[M_NORMAL][i];
    int y = (c[0] * 30 + c[1] * 59 + c[2] * 11) / 100;
    y = y * 85 / 100 + 6;  // a little dimmer, lifted blacks
    pal_[M_IDLE][i][0] = y; pal_[M_IDLE][i][1] = y; pal_[M_IDLE][i][2] = min(255, y + 8);
  }
  memcpy(from_, pal_[M_NORMAL], sizeof from_);
  updateLut(0);
  return true;
}

void Nebula::mood(uint8_t m, float t) {
  if (m == mood_ || m >= M_COUNT) return;
  // Start the fade from whatever is on screen now (possibly mid-fade).
  for (int i = 0; i < 256; i++) {
    uint16_t c = __builtin_bswap16(lut_[i]);
    from_[i][0] = (c >> 8) & 0xF8; from_[i][1] = (c >> 3) & 0xFC; from_[i][2] = (c << 3) & 0xF8;
  }
  mood_ = m;
  moodT0_ = t;
  fading_ = true;
}

void Nebula::updateLut(float t) {
  static constexpr float FADE = 1.0f;
  float k = (t - moodT0_) / FADE;
  if (k >= 1) { k = 1; fading_ = false; }
  if (k < 0) k = 0;
  const uint8_t (*to)[3] = pal_[mood_];
  for (int i = 0; i < 256; i++) {
    int r = from_[i][0] + (to[i][0] - from_[i][0]) * k;
    int g = from_[i][1] + (to[i][1] - from_[i][1]) * k;
    int b = from_[i][2] + (to[i][2] - from_[i][2]) * k;
    lut_[i] = __builtin_bswap16(lgfx::color565(r, g, b));
  }
}

void Nebula::ripple(int x, int y, float t) {
  rip_[ripNext_] = {(float)x, (float)y, t, true};
  ripNext_ = (ripNext_ + 1) % MAXRIP;
}

static inline int wrap(int v, int m) { return ((v % m) + m) % m; }

void Nebula::draw(lgfx::LGFX_Sprite& dst, float t) {
  // Live ripples: an expanding ring plus a glow at the touch point, both fading out.
  static constexpr float RING_W = 14.0f, GLOW_R = 44.0f;
  struct Live { float x, y, r, ring, glow; } live[MAXRIP];
  int nlive = 0;
  for (auto& r : rip_) {
    if (!r.on) continue;
    float age = t - r.t0;
    if (age > LIFE || age < 0) { r.on = false; continue; }
    float fade = 1.0f - age / LIFE;
    fade *= fade;
    live[nlive++] = {r.x, r.y, age * 150.0f, fade * 150.0f, fade * 130.0f};
  }

  uint16_t* px = (uint16_t*)dst.getBuffer();
  int ax = wrap((int)(t * 6.0f), W), ay = wrap((int)(t * 3.0f), H);
  int bx = wrap((int)(-t * 10.0f), W), by = wrap((int)(t * 5.0f), H);
  int breathe = (int)(10.0f * sinf(t * 0.25f));
  if (fading_) updateLut(t);

  for (int y = 0; y < H; y++) {
    int lo = W, hi = -1;  // boost_ span touched on this row
    for (int k = 0; k < nlive; k++) {
      const Live& L = live[k];
      float dy = y - L.y, dy2 = dy * dy;
      float outer = L.r + RING_W;
      if (dy2 < outer * outer) {
        int xo = (int)sqrtf(outer * outer - dy2);
        float inner = L.r - RING_W;
        int xi = inner > 0 && inner * inner > dy2 ? (int)sqrtf(inner * inner - dy2) : -1;
        int cx = (int)L.x;
        int x0 = max(0, cx - xo), x1 = min(W - 1, cx + xo);
        for (int x = x0; x <= x1; x++) {
          if (xi >= 0 && x > cx - xi && x < cx + xi) { x = cx + xi - 1; continue; }  // skip the hole
          float dx = x - L.x;
          float u = (sqrtf(dx * dx + dy2) - L.r) * (1.0f / RING_W);
          if (u > -1 && u < 1) boost_[x] += (int16_t)(L.ring * (1 - u * u) * (1 - u * u));
        }
        if (x0 < lo) lo = x0;
        if (x1 > hi) hi = x1;
      }
      if (dy2 < GLOW_R * GLOW_R) {
        int xg = (int)sqrtf(GLOW_R * GLOW_R - dy2);
        int x0 = max(0, (int)L.x - xg), x1 = min(W - 1, (int)L.x + xg);
        for (int x = x0; x <= x1; x++) {
          float dx = x - L.x;
          float g = 1 - (dx * dx + dy2) * (1.0f / (GLOW_R * GLOW_R));
          if (g > 0) boost_[x] += (int16_t)(L.glow * g * g);
        }
        if (x0 < lo) lo = x0;
        if (x1 > hi) hi = x1;
      }
    }

    const uint8_t* ra = a_ + wrap(y + ay, H) * W;
    const uint8_t* rb = b_ + wrap(y + by, H) * W;
    uint16_t* out = px + y * W;
    int xa = ax, xb = bx;
    for (int x = 0; x < W; x++) {
      int v = ((ra[xa] * 5 + rb[xb] * 3) >> 3) + breathe;
      if (++xa == W) xa = 0;
      if (++xb == W) xb = 0;
      out[x] = lut_[v < 0 ? 0 : v > 255 ? 255 : v];
    }
    if (hi >= lo) {
      for (int x = lo; x <= hi; x++) {
        int v = ((ra[wrap(x + ax, W)] * 5 + rb[wrap(x + bx, W)] * 3) >> 3) + breathe + boost_[x];
        out[x] = lut_[v < 0 ? 0 : v > 255 ? 255 : v];
        boost_[x] = 0;
      }
    }
  }

  // Stars: fixed positions, slow twinkle.
  for (int i = 0; i < 46; i++) {
    int sx = (int)(hashp(i, 91, 0) * W);
    int sy = (int)(hashp(i, 37, 0) * H);
    float tw = 0.5f + 0.5f * sinf(t * (0.6f + hashp(i, 5, 0) * 1.6f) + i);
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
