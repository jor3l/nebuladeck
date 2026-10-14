#include "ui.h"
#include <vector>
#include "version.h"
#include "icons.h"

// Soft glow around (cx, cy): light falling off smoothly from the centre to rOut, with the
// core slightly darkened so a logo on top stays readable. strength 0-1. No edges, no ring.
static void drawGlow(LGFX_Sprite& fb, int cx, int cy, float rOut, uint8_t r, uint8_t g, uint8_t b, float strength) {
  static constexpr float CORE = 24;
  auto* buf = (uint16_t*)fb.getBuffer();
  int w = fb.width(), h = fb.height(), R = (int)rOut + 1;
  for (int y = max(0, cy - R); y < min(h, cy + R); y++) {
    for (int x = max(0, cx - R); x < min(w, cx + R); x++) {
      float d = sqrtf((float)((x - cx) * (x - cx) + (y - cy) * (y - cy)));
      if (d >= rOut) continue;
      float glow = (1 - d / rOut) * (1 - d / rOut) * strength;
      float dark = d < CORE ? 0.3f * (1 - (d / CORE) * (d / CORE)) : 0;
      uint16_t c = __builtin_bswap16(buf[y * w + x]);
      float br = (c >> 8) & 0xF8, bg = (c >> 3) & 0xFC, bb = (c << 3) & 0xF8;
      br += (r - br) * glow; bg += (g - bg) * glow; bb += (b - bb) * glow;
      br += (10 - br) * dark; bg += (12 - bg) * dark; bb += (28 - bb) * dark;
      buf[y * w + x] = __builtin_bswap16(lgfx::color565((int)br, (int)bg, (int)bb));
    }
  }
}

// Light glowing in from all four screen edges (the presence screen's "breathing").
static void drawEdgeGlow(LGFX_Sprite& fb, uint8_t r, uint8_t g, uint8_t b, float strength) {
  static constexpr int E = 44;  // band width
  static uint8_t fall[E];       // falloff by distance from the edge, 0-255
  if (!fall[0]) for (int d = 0; d < E; d++) fall[d] = 255 * (1 - d / (float)E) * (1 - d / (float)E);
  auto* buf = (uint16_t*)fb.getBuffer();
  int w = fb.width(), h = fb.height(), s = (int)(strength * 256);
  auto blend = [&](int x, int y, int d) {
    int a = fall[d] * s >> 8;
    uint16_t c = __builtin_bswap16(buf[y * w + x]);
    int br = (c >> 8) & 0xF8, bg = (c >> 3) & 0xFC, bb = (c << 3) & 0xF8;
    buf[y * w + x] = __builtin_bswap16(lgfx::color565(br + (r - br) * a / 255, bg + (g - bg) * a / 255, bb + (b - bb) * a / 255));
  };
  for (int y = 0; y < h; y++) {
    int dy = min(y, h - 1 - y);
    if (dy < E) {
      for (int x = 0; x < w; x++) blend(x, y, min(dy, min(x, w - 1 - x)));  // top/bottom bands
    } else {
      for (int x = 0; x < E; x++) { blend(x, y, x); blend(w - 1 - x, y, x); }  // sides
    }
  }
}

// Logo for a known agent name ("Claude", "Codex", "Gemini CLI"...), or nullptr.
// Cached: it's called every frame and only recomputed when the name changes.
static const AgentIcon* iconFor(const String& agent) {
  static String last;
  static const AgentIcon* hit = nullptr;
  if (agent == last) return hit;
  last = agent;
  hit = nullptr;
  String a = agent;
  a.toLowerCase();
  for (const AgentIcon& ic : AGENT_ICONS) {
    const char* p = ic.names;
    while (*p) {
      const char* e = strchr(p, '|');
      int len = e ? e - p : strlen(p);
      if (a.indexOf(String(p).substring(0, len)) >= 0) return hit = &ic;
      p += len + (e ? 1 : 0);
    }
  }
  return nullptr;
}

// Tints an n x n alpha mask onto the framebuffer, centred on (cx, cy).
static void drawMask(LGFX_Sprite& fb, const uint8_t* m, int n, int cx, int cy, uint8_t r, uint8_t g, uint8_t b) {
  auto* buf = (uint16_t*)fb.getBuffer();
  int x0 = cx - n / 2, y0 = cy - n / 2, w = fb.width(), h = fb.height();
  for (int y = 0; y < n; y++) {
    int py = y0 + y;
    if (py < 0 || py >= h) continue;
    for (int x = 0; x < n; x++) {
      int a = m[y * n + x], px = x0 + x;
      if (!a || px < 0 || px >= w) continue;
      uint16_t c = __builtin_bswap16(buf[py * w + px]);  // the sprite stores RGB565 byte-swapped
      int br = (c >> 8) & 0xF8, bg = (c >> 3) & 0xFC, bb = (c << 3) & 0xF8;
      br += (r - br) * a / 255; bg += (g - bg) * a / 255; bb += (b - bb) * a / 255;
      buf[py * w + px] = __builtin_bswap16(lgfx::color565(br, bg, bb));
    }
  }
}

static const int W = 320, H = 480;
// Live/presence accent, matched to the nebula's violet.
static const uint8_t PURPLE_R = 160, PURPLE_G = 100, PURPLE_B = 255;

static uint16_t statusColor(LovyanGFX& g, uint8_t s) {
  switch (s) {
    case ST_WORKING: return g.color565(60, 200, 255);
    case ST_WAITING: return g.color565(255, 190, 60);
    case ST_DONE: return g.color565(70, 220, 140);
    case ST_ERROR: return g.color565(255, 90, 100);
    default: return g.color565(120, 140, 180);
  }
}

static const char* statusLabel(uint8_t s) {
  switch (s) {
    case ST_WORKING: return "WORKING";
    case ST_WAITING: return "NEEDS YOU";
    case ST_DONE: return "DONE";
    case ST_ERROR: return "ERROR";
    default: return "IDLE";
  }
}

static uint16_t levelColor(LovyanGFX& g, uint8_t l) {
  return l == LV_ERROR ? g.color565(255, 90, 100)
         : l == LV_WARN ? g.color565(255, 190, 60)
                        : g.color565(90, 150, 255);
}

// Word-wrapped text; returns lines drawn. Last line ends in "..." when cut.
static int wrapText(LovyanGFX& g, const String& s, int x, int y, int w, int lh, int maxLines) {
  std::vector<String> words;
  int st = 0;
  for (int i = 0; i <= (int)s.length(); i++) {
    if (i == (int)s.length() || s[i] == ' ') {
      if (i > st) words.push_back(s.substring(st, i));
      st = i + 1;
    }
  }
  String line;
  int lines = 0;
  auto flush = [&](bool more) {
    String l = line;
    if (more) {
      while (l.length() > 0 && g.textWidth(l + "...") > w) l.remove(l.length() - 1);
      l += "...";
    }
    g.drawString(l, x, y + lines * lh);
    lines++;
    line = "";
  };
  for (size_t i = 0; i < words.size(); i++) {
    String wd = words[i];
    String trial = line.length() ? line + " " + wd : wd;
    if (g.textWidth(trial) <= w) { line = trial; continue; }
    if (line.length() == 0) {
      while (wd.length() > 1 && g.textWidth(wd) > w) wd.remove(wd.length() - 1);
      line = wd;
      continue;
    }
    if (lines == maxLines - 1) { flush(true); return lines; }
    flush(false);
    i--;
  }
  if (line.length()) flush(false);
  return lines;
}

// Centers on the glyphs (cap height), not the font box, which sits low for FreeFonts.
static void centerText(LovyanGFX& g, const String& s, int cx, int cy) {
  g.setTextDatum(baseline_center);
  g.drawString(s, cx, cy + (int)(g.fontHeight() * 0.31f));
  g.setTextDatum(top_left);
}

static String ago(uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s < 60) return String(s) + "s";
  if (s < 3600) return String(s / 60) + "m";
  return String(s / 3600) + "h";
}

bool Ui::begin() {
  fb_.setPsram(true);
  fb_.setColorDepth(16);
  if (!fb_.createSprite(W, H)) return false;
  return nebula_.begin();
}

void Agent::clear() {
  *this = Agent();
}

void Agent::pushMsg(const String& text, uint8_t level, uint32_t ttl, uint32_t now) {
  for (int i = MAXMSG - 1; i > 0; i--) msgs[i] = msgs[i - 1];
  msgs[0].text = text; msgs[0].level = level; msgs[0].at = now; msgs[0].ttl = ttl; msgs[0].count = 1;
  if (nmsg < MAXMSG) nmsg++;
}

void Agent::pushStep(const String& text, uint32_t now) {
  uint16_t count = 1;
  for (int i = 0; i < nmsg; i++) {
    if (msgs[i].level != LV_STEP) continue;
    if (msgs[i].text == text) count = msgs[i].count + 1;
    for (int j = i; j < nmsg - 1; j++) msgs[j] = msgs[j + 1];  // at most one step line
    nmsg--;
    break;
  }
  pushMsg(text, LV_STEP, 0, now);
  msgs[0].count = count;
}

String Agent::line() const {
  if (asking) return askPrompt;
  if (attn) return presence.length() ? presence : String("Needs you");
  if (completed) return summary.length() ? summary : String("Task completed");
  if (status != ST_IDLE) return title.length() ? title : String("Working");
  return presence.length() ? presence : String("Ready");
}

int Ui::children(const Agent& p) const {
  int n = 0;
  for (const Agent& a : agents)
    n += a.used && a.sub.length() && !p.sub.length() && a.name.equalsIgnoreCase(p.name) && a.project == p.project;
  return n;
}

int Ui::count() const {
  int n = 0;
  for (const Agent& a : agents) n += a.used;
  return n;
}

const Agent* Ui::view() const {
  int n = count();
  if (n == 1) for (const Agent& a : agents) if (a.used) return &a;
  if (n > 1 && focus >= 0 && agents[focus].used) return &agents[focus];
  return nullptr;
}

Agent* Ui::view() { return const_cast<Agent*>(static_cast<const Ui*>(this)->view()); }

bool Ui::anyNeedsYou() const {
  for (const Agent& a : agents) if (a.used && a.needsYou()) return true;
  return false;
}

// Bottom sheet geometry shared by drawing and hit-testing.
static int sheetTop(int nopts) { return H - (110 + nopts * 60 + 8); }
static void optRect(int nopts, int i, int& x, int& y, int& w, int& h) {
  x = 16; w = W - 32; h = 52; y = sheetTop(nopts) + 110 + i * 60;
}

int Ui::hitAsk(int px, int py) const {
  const Agent* v = view();
  if (!v) return -1;
  const Agent& a = *v;
  if (!a.asking) return -1;
  for (int i = 0; i < a.nopts; i++) {
    int x, y, w, h;
    optRect(a.nopts, i, x, y, w, h);
    if (px >= x && px < x + w && py >= y && py < y + h) return i;
  }
  return -1;
}

// Nebula background and the NEBULADECK logo. When "All agents" is showing, the logo sits at
// the top left at ~70% size, so both have their own tap area.
void Ui::header(uint32_t now) {
  nebula_.draw(fb_, now / 1000.0f);
  const Agent* v = view();
  headerK_ = !net.setup && count() > 1 && v ? 1.0f : 0.0f;
  float e = headerK_;
  float size = 1.0f - 0.3f * e;
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextSize(size);
  int w = fb_.textWidth("N E B U L A D E C K");
  // On the card list the logo sits centred between the top edge and the divider (y 70).
  int y0 = !net.setup && count() > 1 && !v ? 35 : 44;
  int cx = (int)(W / 2 + (16 + w / 2 - W / 2) * e), cy = (int)(y0 + (21 - y0) * e);
  fb_.setTextColor(fb_.color565(150, 160, 210));
  centerText(fb_, "N E B U L A D E C K", cx, cy);
  fb_.setTextSize(1);
}

void Ui::drawIdle(uint32_t now) {
  float t = now / 1000.0f;
  header(now);
  float pulse = 0.5f + 0.5f * sinf(t * 1.4f);

  int b = 150 + (int)(pulse * 105);
  fb_.setFont(&fonts::FreeSans12pt7b);
  fb_.setTextColor(fb_.color565(b, b, 255));
  centerText(fb_, net.online ? "Agent not connected" : "Connecting to WiFi", W / 2, H / 2);
  fb_.setFont(&fonts::FreeSans9pt7b);
  fb_.setTextColor(fb_.color565(120, 130, 180));
  if (net.online) {
    centerText(fb_, "waiting for an agent", W / 2, H / 2 + 30);
  } else {
    centerText(fb_, net.ssid, W / 2, H / 2 + 30);
    fb_.setTextColor(fb_.color565(90, 100, 150));
    centerText(fb_, "Hold the screen to change network", W / 2, H - 40);
  }
}

void Ui::drawPresence(uint32_t now) {
  const Agent& a = *view();
  float t = now / 1000.0f;
  header(now);
  uint16_t dot = a.attn ? fb_.color565(255, 190, 60) : fb_.color565(PURPLE_R, PURPLE_G, PURPLE_B);
  float pulse = 0.5f + 0.5f * sinf(t * (a.attn ? 4.5f : 1.6f));
  int cx = W / 2, cy = H / 2 - 36;
  // The screen edges breathe: purple, or amber when the agent needs you (the nebula turns amber too).
  drawEdgeGlow(fb_, a.attn ? 255 : PURPLE_R, a.attn ? 170 : PURPLE_G, a.attn ? 50 : PURPLE_B,
               a.attn ? 0.45f + 0.4f * pulse : 0.25f + 0.3f * pulse);
  // Known agents: their logo (on a softly darkened spot so it reads on any nebula). Others: a dot in a ring.
  if (const AgentIcon* ic = iconFor(a.name)) {
    drawGlow(fb_, cx, cy, 30, 0, 0, 0, 0);
    drawMask(fb_, ic->big, ICON_BIG, cx, cy, ic->r, ic->g, ic->b);
  } else {
    fb_.drawCircle(cx, cy, 22 + (int)(pulse * 10), a.attn ? fb_.color565(120, 90, 30) : fb_.color565(70, 40, 130));
    fb_.fillCircle(cx, cy, 9 + (int)(pulse * 3), dot);
  }

  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  centerText(fb_, a.sub.length() ? displayName(a) : a.name + " connected", W / 2, cy + 58);

  // Secondary line; a trailing "..." animates.
  String text = a.presence.length() ? a.presence : "Ready";
  bool busy = text.endsWith("...");
  if (busy) text.remove(text.length() - 3);
  int lum = busy ? 170 + (int)(pulse * 85) : 190;
  fb_.setFont(&fonts::FreeSans12pt7b);
  fb_.setTextColor(fb_.color565(lum * 6 / 10, lum * 8 / 10, lum));
  if (busy) {
    String dots = String("...").substring(0, 1 + (now / 400) % 3);
    int full = fb_.textWidth(text + "...");
    fb_.setTextDatum(baseline_left);
    fb_.drawString(text + dots, (W - full) / 2, cy + 96 + (int)(fb_.fontHeight() * 0.31f));
    fb_.setTextDatum(top_left);
  } else {
    centerText(fb_, text, W / 2, cy + 96);
  }
}

static String compact(uint32_t n) {
  if (n < 1000) return String(n);
  if (n < 100000) return String(n / 1000.0f, 1) + "k";
  if (n < 1000000) return String(n / 1000) + "k";
  return String(n / 1000000.0f, 2) + "M";
}

void Ui::drawDone(uint32_t now) {
  const Agent& a = *view();
  float t = now / 1000.0f;
  header(now);
  uint16_t green = fb_.color565(70, 220, 140);

  // Check mark in a ring.
  int cx = W / 2, cy = 120;
  fb_.fillCircle(cx, cy, 38, fb_.color565(8, 30, 28));
  fb_.drawCircle(cx, cy, 38, green);
  fb_.drawCircle(cx, cy, 37, green);
  fb_.drawWideLine(cx - 15, cy + 1, cx - 4, cy + 13, 6, green);
  fb_.drawWideLine(cx - 4, cy + 13, cx + 17, cy - 12, 6, green);

  // Completion message.
  fb_.setTextDatum(top_center);
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  wrapText(fb_, a.summary.length() ? a.summary : "Task completed", W / 2, 180, W - 50, 28, 4);
  fb_.setTextDatum(top_left);

  // Agent idle.
  int pr = 4 + (int)(1.5f * (0.5f + 0.5f * sinf(t * 2.0f)));
  fb_.fillCircle(W / 2 - 52, 318, pr, fb_.color565(120, 130, 180));
  fb_.setFont(&fonts::FreeSans9pt7b);
  fb_.setTextColor(fb_.color565(150, 160, 210));
  fb_.setTextDatum(middle_left);
  fb_.drawString("Agent idle", W / 2 - 36, 317);
  fb_.setTextDatum(top_left);

  // Floating token card (only when the agent reported totals).
  if (a.tokIn == 0 && a.tokOut == 0) return;
  int x = 16, y = 358, w = W - 32, h = 98;
  fb_.fillRoundRect(x + 2, y + 6, w - 4, h, 18, fb_.color565(1, 2, 8));  // shadow
  fb_.fillRoundRect(x, y, w, h, 18, fb_.color565(14, 17, 40));
  fb_.drawRoundRect(x, y, w, h, 18, fb_.color565(46, 56, 112));
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextColor(fb_.color565(100, 110, 160));
  fb_.drawString("TOKENS", x + 20, y + 12);
  if (a.durationS >= 0) {
    fb_.setTextDatum(top_right);
    fb_.drawString(ago((uint32_t)a.durationS * 1000UL), x + w - 20, y + 12);
    fb_.setTextDatum(top_left);
  }
  fb_.drawFastVLine(W / 2, y + 40, 44, fb_.color565(46, 56, 112));
  const char* labels[2] = {"IN", "OUT"};
  uint32_t vals[2] = {a.tokIn, a.tokOut};
  uint16_t cols[2] = {fb_.color565(60, 200, 255), fb_.color565(255, 130, 200)};
  for (int i = 0; i < 2; i++) {
    int mx = i == 0 ? W / 4 + 8 : 3 * W / 4 - 8;
    fb_.setFont(&fonts::FreeSansBold18pt7b);
    fb_.setTextColor(cols[i]);
    centerText(fb_, compact(vals[i]), mx, y + 56);
    fb_.setFont(&fonts::FreeSans9pt7b);
    fb_.setTextColor(fb_.color565(120, 130, 180));
    centerText(fb_, labels[i], mx, y + 84);
  }
}

void Ui::drawStatus(uint32_t now) {
  const Agent& a = *view();
  float t = now / 1000.0f;
  uint16_t sc = statusColor(fb_, a.status);
  fb_.fillScreen(fb_.color565(7, 9, 20));
  fb_.setTextDatum(top_left);

  // Header: live dot + project.
  int pr = 5 + (int)(2.0f * (0.5f + 0.5f * sinf(t * 3.0f)));
  if (const AgentIcon* ic = iconFor(a.name)) drawMask(fb_, ic->small, ICON_SMALL, 24, 26, ic->r, ic->g, ic->b);
  else fb_.fillCircle(24, 26, pr, fb_.color565(PURPLE_R, PURPLE_G, PURPLE_B));
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(230, 235, 255));
  fb_.drawString(a.project.length() ? a.project : "NebulaDeck", 44, 12);

  // Hero card.
  fb_.fillRoundRect(16, 56, 288, 160, 16, fb_.color565(16, 20, 42));
  fb_.drawRoundRect(16, 56, 288, 160, 16, fb_.color565(36, 44, 88));
  fb_.fillRoundRect(32, 74, 6, 16, 3, sc);
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextColor(sc);
  fb_.drawString(statusLabel(a.status), 46, 72);
  fb_.setFont(&fonts::FreeSansBold18pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  wrapText(fb_, a.title.length() ? a.title : "Ready", 32, 100, 256, 32, 3);

  // Progress / activity bar.
  if (a.progress >= 0 || a.status == ST_WORKING) {
    int bx = 32, by = 198, bw = 256, bh = 6;
    fb_.fillRoundRect(bx, by, bw, bh, 3, fb_.color565(32, 38, 72));
    if (a.progress >= 0) {
      int fw = (int)(bw * min(a.progress, 1.0f));
      if (fw > 0) fb_.fillRoundRect(bx, by, max(fw, bh), bh, 3, sc);
    } else {
      int seg = 70;
      int sx = bx + (int)((bw - seg) * (0.5f + 0.5f * sinf(t * 2.2f)));
      fb_.fillRoundRect(sx, by, seg, bh, 3, sc);
    }
  }

  // Activity feed.
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextColor(fb_.color565(100, 110, 160));
  fb_.drawString("ACTIVITY", 20, 230);
  if (!a.nmsg) {
    fb_.setFont(&fonts::FreeSans9pt7b);
    fb_.setTextColor(fb_.color565(80, 90, 130));
    fb_.drawString("Steps and milestones show up here", 20, 258);
  }
  for (int i = 0; i < a.nmsg && i < 4; i++) {
    int y = 254 + i * 54;
    bool step = a.msgs[i].level == LV_STEP;
    fb_.fillRoundRect(16, y, 288, 48, 10, step ? fb_.color565(11, 14, 30) : fb_.color565(16, 20, 42));
    fb_.fillRoundRect(16, y + 6, 4, 36, 2, step ? fb_.color565(PURPLE_R / 2, PURPLE_G / 2, PURPLE_B / 2) : levelColor(fb_, a.msgs[i].level));
    fb_.setFont(&fonts::FreeSans9pt7b);
    fb_.setTextColor(step ? fb_.color565(150, 158, 195) : fb_.color565(215, 222, 245));
    wrapText(fb_, step && a.msgs[i].count > 1 ? a.msgs[i].text + "  x" + String(a.msgs[i].count) : a.msgs[i].text, 30, y + 4, 232, 20, 2);
    if (step) {
      // In progress: three dots chasing each other, no timer.
      for (int k = 0; k < 3; k++) {
        float ph = 0.5f + 0.5f * sinf(t * 5.0f - k * 0.9f);
        fb_.fillCircle(276 + k * 9, y + 12, 2 + (int)(ph * 1.5f), fb_.color565(80 + (int)(ph * 80), 60 + (int)(ph * 40), 140 + (int)(ph * 100)));
      }
    } else {
      // Done events: when they happened, from the start of the task ("+2:14"). Fixed.
      uint32_t s = (int32_t)(a.msgs[i].at - a.taskStart) > 0 ? (a.msgs[i].at - a.taskStart) / 1000 : 0;
      char stamp[12];
      if (s < 3600) snprintf(stamp, sizeof stamp, "+%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
      else snprintf(stamp, sizeof stamp, "+%uh%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60));
      fb_.setTextDatum(top_right);
      fb_.setTextColor(fb_.color565(100, 110, 160));
      fb_.drawString(stamp, 298, y + 4);
      fb_.setTextDatum(top_left);
    }
  }
}

void Ui::drawAsk(uint32_t) {
  const Agent& a = *view();
  fb_.fillRectAlpha(0, 0, W, H, 170, fb_.color565(2, 3, 10));
  int top = sheetTop(a.nopts);
  fb_.fillRoundRect(0, top, W, H - top + 20, 22, fb_.color565(20, 25, 52));
  fb_.fillRoundRect(W / 2 - 22, top + 8, 44, 4, 2, fb_.color565(70, 80, 130));
  fb_.setTextDatum(top_left);
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  wrapText(fb_, a.askPrompt, 20, top + 22, W - 40, 26, 3);
  for (int i = 0; i < a.nopts; i++) {
    int x, y, w, h;
    optRect(a.nopts, i, x, y, w, h);
    uint16_t c = i == 0 ? fb_.color565(60, 200, 255) : fb_.color565(36, 44, 88);
    fb_.fillRoundRect(x, y, w, h, 14, c);
    fb_.setFont(&fonts::FreeSansBold12pt7b);
    fb_.setTextColor(i == 0 ? fb_.color565(4, 12, 28) : fb_.color565(230, 235, 255));
    centerText(fb_, a.opts[i], x + w / 2, y + h / 2);
  }
}

// Welcome screen: QR to the setup page, then BLE progress from the page.
static const int QR_CARD = 212, QR_Y = 138;

bool Ui::hitCancel(int px, int py) const {
  return net.setup && net.canCancel && px > W / 2 - 80 && px < W / 2 + 80 && py > H - 44;
}

void Ui::drawSetup(uint32_t now) {
  float t = now / 1000.0f;
  header(now);
  fb_.setFont(&fonts::FreeSansBold18pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  centerText(fb_, "Welcome", W / 2, 88);
  fb_.setFont(&fonts::FreeSans9pt7b);
  fb_.setTextColor(fb_.color565(150, 160, 210));
  centerText(fb_, "Scan to connect me to WiFi", W / 2, 118);

  // The QR only changes with the URL; render it once.
  if (qrFor_ != net.url) {
    qr_.setPsram(true);
    qr_.setColorDepth(16);
    if (qr_.width() || qr_.createSprite(QR_CARD - 16, QR_CARD - 16)) {
      qr_.fillScreen(TFT_WHITE);
      qr_.qrcode(net.url.c_str(), 0, 0, qr_.width(), 1, true);
      qrFor_ = net.url;
    }
  }
  int qx = (W - QR_CARD) / 2;
  fb_.fillRoundRect(qx, QR_Y, QR_CARD, QR_CARD, 18, TFT_WHITE);
  if (qr_.width()) qr_.pushSprite(&fb_, qx + 8, QR_Y + 8);
  fb_.setTextColor(fb_.color565(110, 120, 170));
  centerText(fb_, "jor3l.github.io/nebuladeck", W / 2, QR_Y + QR_CARD + 16);

  float pulse = 0.5f + 0.5f * sinf(t * 3.0f);
  uint16_t c = net.tone == T_OK ? fb_.color565(70, 220, 140)
               : net.tone == T_ERR ? fb_.color565(255, 90, 100)
               : net.tone == T_BUSY ? fb_.color565(60, 200, 255)
                                    : fb_.color565(150, 160, 210);
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  int tw = fb_.textWidth(net.status);
  int sy = QR_Y + QR_CARD + 48;
  int dx = (W - tw) / 2 - 12;
  fb_.fillCircle(dx, sy, net.tone == T_BUSY ? 4 + (int)(pulse * 2) : 5, c);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  centerText(fb_, net.status, W / 2 + 8, sy);
  fb_.setFont(&fonts::FreeSans9pt7b);
  fb_.setTextColor(c);
  centerText(fb_, net.detail, W / 2, sy + 26);

  if (net.canCancel) {
    fb_.setTextColor(fb_.color565(120, 130, 180));
    centerText(fb_, "Cancel", W / 2, H - 20);
  }
}

// Long-press feedback on the nebula screens.
void Ui::drawHold() {
  int y = H - 70;
  fb_.fillRoundRect(24, y - 26, W - 48, 64, 14, fb_.color565(20, 25, 52));
  fb_.setFont(&fonts::FreeSans9pt7b);
  fb_.setTextColor(fb_.color565(230, 235, 255));
  centerText(fb_, "Keep holding to set up WiFi", W / 2, y - 6);
  fb_.fillRoundRect(40, y + 14, W - 80, 6, 3, fb_.color565(36, 44, 88));
  fb_.fillRoundRect(40, y + 14, (int)((W - 80) * min(hold, 1.0f)), 6, 3, fb_.color565(60, 200, 255));
}

// Update prompt: a bottom sheet with Update / Skip.
static const int UP_TOP = H - 186, UP_BY = H - 74, UP_BH = 52;

int Ui::hitUpdate(int px, int py) const {
  if (py < UP_TOP) return -2;
  if (py < UP_BY || py > UP_BY + UP_BH) return -1;
  if (px >= 16 && px < W / 2 - 6) return 0;
  if (px >= W / 2 + 6 && px < W - 16) return 1;
  return -1;
}

void Ui::drawUpdatePrompt() {
  // Follows the finger while being swiped down.
  const int UP_TOP = ::UP_TOP + sheetDrag, UP_BY = ::UP_BY + sheetDrag;
  fb_.fillRoundRect(0, UP_TOP, W, H - UP_TOP + 20, 22, fb_.color565(20, 25, 52));
  fb_.fillRoundRect(W / 2 - 22, UP_TOP + 8, 44, 4, 2, fb_.color565(70, 80, 130));
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  centerText(fb_, "Update available", W / 2, UP_TOP + 38);
  fb_.setFont(&fonts::FreeSans9pt7b);
  fb_.setTextColor(fb_.color565(150, 160, 210));
  centerText(fb_, update, W / 2, UP_TOP + 64);
  fb_.setTextColor(fb_.color565(110, 120, 170));
  centerText(fb_, "installed: " FW_VERSION, W / 2, UP_TOP + 86);
  int bw = W / 2 - 22;
  fb_.fillRoundRect(16, UP_BY, bw, UP_BH, 14, fb_.color565(60, 200, 255));
  fb_.fillRoundRect(W / 2 + 6, UP_BY, bw, UP_BH, 14, fb_.color565(36, 44, 88));
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(4, 12, 28));
  centerText(fb_, "Update", 16 + bw / 2, UP_BY + UP_BH / 2);
  fb_.setTextColor(fb_.color565(230, 235, 255));
  centerText(fb_, "Skip", W / 2 + 6 + bw / 2, UP_BY + UP_BH / 2);
}

// Device info sheet, opened by tapping the logo.
static const int IN_TOP = H - 364, IN_BY = H - 74, IN_BH = 52, OFF_BY = H - 136, OFF_BH = 48;

int Ui::hitInfo(int px, int py) const {
  if (py < IN_TOP) return -2;
  if (py >= OFF_BY && py <= OFF_BY + OFF_BH && px >= 16 && px < W - 16) return 2;  // screen off
  if (py >= IN_BY && py <= IN_BY + IN_BH) {
    if (px >= 16 && px < W / 2 - 6) return 0;
    if (px >= W / 2 + 6 && px < W - 16) return 1;
  }
  return -1;
}

void Ui::drawInfo(uint32_t now) {
  const int IN_TOP = ::IN_TOP + sheetDrag, IN_BY = ::IN_BY + sheetDrag, OFF_BY = ::OFF_BY + sheetDrag;
  fb_.fillRectAlpha(0, 0, W, H, 150, fb_.color565(2, 3, 10));
  fb_.fillRoundRect(0, IN_TOP, W, H - IN_TOP + 20, 22, fb_.color565(20, 25, 52));
  fb_.fillRoundRect(W / 2 - 22, IN_TOP + 8, 44, 4, 2, fb_.color565(70, 80, 130));
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  centerText(fb_, "NebulaDeck", W / 2, IN_TOP + 38);

  bool local = !strcmp(FW_CHANNEL, "local");
  String upd = updateCheck == UC_CHECKING ? "Checking..."
               : update.length() ? update + " available"
               : local ? "Off (local build)"
               : updateCheck == UC_FAILED ? "Check failed"
               : updateCheck == UC_CURRENT ? "Up to date"
                                            : "Not checked yet";
  const char* labels[] = {"Version", "Channel", "Network", "Address", "Updates"};
  String values[] = {
      FW_VERSION,
      FW_CHANNEL,
      net.online ? net.ssid + "  " + String(net.rssi) + " dBm" : net.ssid.length() ? "Joining " + net.ssid : "Not set up",
      net.online ? net.ip + "  " + net.host : "-",
      upd,
  };
  fb_.setFont(&fonts::FreeSans9pt7b);
  for (int i = 0; i < 5; i++) {
    int y = IN_TOP + 70 + i * 30;
    fb_.setTextColor(fb_.color565(120, 130, 180));
    fb_.setTextDatum(middle_left);
    fb_.drawString(labels[i], 20, y);
    fb_.setTextColor(fb_.color565(230, 235, 255));
    fb_.setTextDatum(middle_right);
    String v = values[i];
    while (v.length() > 1 && fb_.textWidth(v) > W - 120) v.remove(v.length() - 1);
    fb_.drawString(v, W - 20, y);
    if (i < 4) fb_.drawFastHLine(20, y + 15, W - 40, fb_.color565(32, 38, 76));
  }
  fb_.setTextDatum(top_left);

  int bw = W / 2 - 22;
  bool busy = updateCheck == UC_CHECKING || local;
  fb_.fillRoundRect(16, IN_BY, bw, IN_BH, 14, busy ? fb_.color565(36, 44, 88) : fb_.color565(60, 200, 255));
  bool armed = forgetArmed && now - forgetArmed < 4000;
  fb_.fillRoundRect(W / 2 + 6, IN_BY, bw, IN_BH, 14, armed ? fb_.color565(255, 90, 100) : fb_.color565(36, 44, 88));
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextColor(busy ? fb_.color565(120, 130, 180) : fb_.color565(4, 12, 28));
  centerText(fb_, update.length() ? "Install update" : "Check updates", 16 + bw / 2, IN_BY + IN_BH / 2);
  // Screen off: stays dark until a tap; the device keeps receiving and saving meanwhile.
  fb_.fillRoundRect(16, OFF_BY, W - 32, OFF_BH, 14, fb_.color565(28, 30, 62));
  fb_.drawRoundRect(16, OFF_BY, W - 32, OFF_BH, 14, fb_.color565(60, 56, 120));
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextColor(fb_.color565(220, 225, 250));
  centerText(fb_, "Turn screen off", W / 2, OFF_BY + OFF_BH / 2);
  fb_.setTextColor(armed ? fb_.color565(20, 4, 8) : fb_.color565(255, 140, 150));
  centerText(fb_, armed ? "Tap to confirm" : "Forget WiFi", W / 2 + 6 + bw / 2, IN_BY + IN_BH / 2);
}

void Ui::drawUpdating() {
  fb_.fillScreen(fb_.color565(4, 5, 16));
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextColor(fb_.color565(150, 160, 210));
  centerText(fb_, "N E B U L A D E C K", W / 2, 44);
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  centerText(fb_, updateError.length() ? "Update failed" : "Updating to " + update, W / 2, H / 2 - 30);
  fb_.setFont(&fonts::FreeSans9pt7b);
  if (updateError.length()) {
    fb_.setTextColor(fb_.color565(255, 90, 100));
    fb_.setTextDatum(top_left);
    wrapText(fb_, updateError, 30, H / 2, W - 60, 22, 3);
  } else {
    int bw = W - 80, pct = updatePct < 0 ? 0 : updatePct;
    fb_.fillRoundRect(40, H / 2, bw, 8, 4, fb_.color565(36, 44, 88));
    fb_.fillRoundRect(40, H / 2, bw * pct / 100, 8, 4, fb_.color565(60, 200, 255));
    fb_.setTextColor(fb_.color565(150, 160, 210));
    centerText(fb_, String(pct) + "%   keep it plugged in", W / 2, H / 2 + 36);
  }
  fb_.pushSprite(&lcd_, 0, 0);
}

// Card list (2+ agents): one card per agent over the nebula, scrollable.
static const int LIST_Y = 88, CARD_H = 100, CARD_GAP = 10;

// Text cut to a width, ending in "..." when it doesn't fit.
static String fit(LovyanGFX& g, String s, int w) {
  if (g.textWidth(s) <= w) return s;
  while (s.length() > 1 && g.textWidth(s + "...") > w) s.remove(s.length() - 1);
  return s + "...";
}

int Ui::maxScroll() const {
  int idx[MAXAGENTS], n = cardOrder(idx), total = 0;
  for (int k = 0; k < n; k++) total += cardHeight(idx[k]) + CARD_GAP;
  return max(0, total - CARD_GAP - (H - LIST_Y - 8));
}

// Busy: a task running, or a hook status line still in progress ("Running a command...").
static bool busy(const Agent& a) {
  return !a.completed && (a.status != ST_IDLE || a.presence.endsWith("..."));
}

// The colour of a card's status word (and of a subagent row's dot).
uint16_t Ui::labelColor(LovyanGFX& g, const Agent& a, bool hot) {
  if (a.restored) return g.color565(110, 115, 150);
  if (hot) return g.color565(255, 200, 90);
  if (a.completed) return g.color565(70, 220, 140);
  if (a.status == ST_IDLE && busy(a)) return statusColor(g, ST_WORKING);
  return statusColor(g, a.status);
}

// Hook-only agents have no task status: their status line says whether they're busy.
const char* Ui::cardLabel(const Agent& a) {
  if (a.restored) return "RECONNECTING";
  return a.asking ? "QUESTION" : a.attn ? "NEEDS YOU" : a.completed ? "DONE"
         : a.status != ST_IDLE ? statusLabel(a.status) : busy(a) ? "ACTIVE" : "READY";
}

// The session a subagent belongs to (same name and project), or -1.
int Ui::parentOf(const Agent& c) const {
  for (int i = 0; i < MAXAGENTS; i++) {
    const Agent& p = agents[i];
    if (p.used && !p.sub.length() && p.name.equalsIgnoreCase(c.name) && p.project == c.project) return i;
  }
  return -1;
}

// Cards by attention: needs you (itself or a subagent), busy, the rest. Subagents are rows
// inside their session's card; one whose session has no card gets its own.
int Ui::cardOrder(int* idx) const {
  auto rank = [&](int i) {
    const Agent& a = agents[i];
    bool kidNeeds = false;
    for (const Agent& c : agents) kidNeeds |= c.used && c.sub.length() && parentOf(c) == i && c.needsYou();
    return a.needsYou() || kidNeeds ? 0 : busy(a) ? 1 : 2;
  };
  int n = 0;
  for (int pass = 0; pass < 3; pass++)
    for (int i = 0; i < MAXAGENTS; i++) {
      const Agent& a = agents[i];
      if (!a.used || (a.sub.length() && parentOf(a) >= 0) || rank(i) != pass) continue;
      idx[n++] = i;
    }
  return n;
}

// A card's height: the session's own content plus one row per running subagent.
static const int SUB_ROW = 26;
int Ui::cardHeight(int i) const {
  int kids = 0;
  for (const Agent& c : agents) kids += c.used && c.sub.length() && parentOf(c) == i;
  return CARD_H + (kids ? 4 + kids * SUB_ROW : 0);
}

int Ui::hitCard(int px, int py) const {
  if (py < LIST_Y) return -1;
  int idx[MAXAGENTS], n = cardOrder(idx), y = LIST_Y - scroll;
  for (int k = 0; k < n; k++) {
    int h = cardHeight(idx[k]);
    if (py >= y && py < y + h && px >= 12 && px < W - 12) return idx[k];
    y += h + CARD_GAP;
  }
  return -1;
}

void Ui::drawAgents(uint32_t now) {
  float t = now / 1000.0f;
  header(now);
  // Divider between the logo and the cards: violet, fading out towards the sides.
  for (int x = 24; x < W - 24; x++) {
    float f = 1.0f - fabsf(x - W / 2) / (W / 2 - 24.0f);
    fb_.drawPixel(x, 70, fb_.color565(20 + (int)(70 * f), 18 + (int)(48 * f), 40 + (int)(120 * f)));
  }
  scroll = max(0, min(scroll, maxScroll()));

  fb_.setClipRect(0, LIST_Y - 2, W, H - LIST_Y + 2);
  int idx[MAXAGENTS], nc = cardOrder(idx), next = LIST_Y - scroll;
  for (int k = 0; k < nc; k++) {
    const Agent& a = agents[idx[k]];
    int ch = cardHeight(idx[k]);
    int x = 12, w = W - 24, y = next;
    next += ch + CARD_GAP;
    if (idx[k] == swipeIdx) x += swipeDx;  // follows the finger while swiped
    if (y > H || y + ch < LIST_Y) continue;
    bool kidNeeds = false;
    for (const Agent& c : agents) kidNeeds |= c.used && c.sub.length() && parentOf(c) == idx[k] && c.needsYou();
    bool hot = a.needsYou() || kidNeeds;
    float pulse = 0.5f + 0.5f * sinf(t * 4.5f);
    // A card that needs you turns amber and pulses; the rest stay dark violet.
    uint16_t bg = hot ? fb_.color565(46 + (int)(pulse * 26), 28 + (int)(pulse * 12), 8)
                  : a.restored ? fb_.color565(10, 11, 24) : fb_.color565(14, 16, 38);
    uint16_t edge = hot ? fb_.color565(255, 170 + (int)(pulse * 40), 50) : a.restored ? fb_.color565(32, 30, 60) : fb_.color565(48, 40, 100);
    fb_.fillRoundRect(x, y, w, ch, 16, bg);
    fb_.drawRoundRect(x, y, w, ch, 16, edge);
    if (hot) fb_.drawRoundRect(x + 1, y + 1, w - 2, ch - 2, 15, edge);

    // Logo (or a dot), name and project, status on the right.
    if (const AgentIcon* ic = iconFor(a.name)) drawMask(fb_, ic->small, ICON_SMALL, x + 24, y + 24, ic->r, ic->g, ic->b);
    else fb_.fillCircle(x + 24, y + 24, 6, fb_.color565(PURPLE_R, PURPLE_G, PURPLE_B));
    bool active = a.status == ST_IDLE && busy(a);
    const char* label = cardLabel(a);
    uint16_t lc = labelColor(fb_, a, a.needsYou());
    fb_.setFont(&fonts::FreeSansBold9pt7b);
    int lw = fb_.textWidth(label);
    fb_.setTextColor(lc);
    fb_.setTextDatum(middle_right);
    fb_.drawString(label, x + w - 14, y + 24);
    fb_.setTextDatum(middle_left);
    fb_.setTextColor(fb_.color565(240, 243, 255));
    String who = fit(fb_, displayName(a), w - 70 - lw);
    fb_.drawString(who, x + 44, y + 24);
    if (a.project.length()) {
      int ww = fb_.textWidth(who);
      fb_.setFont(&fonts::FreeSans9pt7b);
      fb_.setTextColor(fb_.color565(130, 140, 190));
      fb_.drawString(fit(fb_, "  " + a.project, w - 70 - lw - ww), x + 44 + ww, y + 24);
    }

    // What it's doing, then its latest activity.
    fb_.setFont(&fonts::FreeSans9pt7b);
    fb_.setTextColor(hot ? fb_.color565(255, 236, 200) : fb_.color565(215, 222, 245));
    fb_.drawString(fit(fb_, a.line(), w - 30), x + 16, y + 52);
    String act;
    for (int m = 0; m < a.nmsg && !act.length(); m++) act = a.msgs[m].text;
    if (act.length() && !a.asking) {
      fb_.setTextColor(fb_.color565(120, 130, 175));
      fb_.drawString(fit(fb_, act, w - 30), x + 16, y + 74);
    }
    fb_.setTextDatum(top_left);
    if (a.progress >= 0 && !a.completed) {
      int bw = w - 32, fw = (int)(bw * min(a.progress, 1.0f));
      fb_.fillRoundRect(x + 16, y + CARD_H - 10, bw, 4, 2, fb_.color565(32, 38, 72));
      if (fw > 0) fb_.fillRoundRect(x + 16, y + CARD_H - 10, max(fw, 4), 4, 2, statusColor(fb_, a.status));
    }

    // Its running subagents, one compact row each: icon, name, a dot in the colour its status
    // word would have.
    int ry = y + CARD_H + 4;
    for (const Agent& c : agents) {
      if (!c.used || !c.sub.length() || parentOf(c) != idx[k]) continue;
      fb_.drawFastHLine(x + 14, ry - 2, w - 28, fb_.color565(36, 34, 76));
      int cy = ry + SUB_ROW / 2 - 1;
      if (const AgentIcon* ic = iconFor(c.name)) drawMask(fb_, ic->small, ICON_SMALL, x + 26, cy, ic->r, ic->g, ic->b);
      else fb_.fillCircle(x + 26, cy, 5, fb_.color565(PURPLE_R, PURPLE_G, PURPLE_B));
      fb_.setFont(&fonts::FreeSans9pt7b);
      fb_.setTextDatum(middle_left);
      fb_.setTextColor(fb_.color565(200, 206, 236));
      fb_.drawString(fit(fb_, c.role.length() ? c.role : String("agent"), w - 90), x + 44, cy);
      fb_.setTextDatum(top_left);
      bool cNeeds = c.needsYou();
      int r = cNeeds ? 5 + (int)(pulse * 1.5f) : 5;
      fb_.fillCircle(x + w - 22, cy, r, labelColor(fb_, c, cNeeds));
      ry += SUB_ROW;
    }
  }
  fb_.clearClipRect();

  // Scroll hint: a thin bar on the right when the list is longer than the screen.
  int ms = maxScroll();
  if (ms > 0) {
    int track = H - LIST_Y - 16, len = max(30, track * (H - LIST_Y) / (H - LIST_Y + ms));
    int y = LIST_Y + (track - len) * scroll / ms;
    fb_.fillRoundRect(W - 5, y, 3, len, 1, fb_.color565(90, 80, 150));
  }
}

// "< All agents" pill (top right) on an opened agent's screens; amber dot if another needs you.
void Ui::drawBack() {
  int x = W - 134, y = 6, w = 128, h = 30;
  fb_.fillRoundRect(x, y, w, h, 15, fb_.color565(20, 22, 50));
  fb_.drawRoundRect(x, y, w, h, 15, fb_.color565(60, 56, 120));
  uint16_t c = fb_.color565(200, 205, 240);
  fb_.drawLine(x + 18, y + 9, x + 12, y + 15, c); fb_.drawLine(x + 12, y + 15, x + 18, y + 21, c);
  fb_.drawLine(x + 19, y + 9, x + 13, y + 15, c); fb_.drawLine(x + 13, y + 15, x + 19, y + 21, c);
  fb_.setFont(&fonts::FreeSans9pt7b);
  fb_.setTextColor(c);
  fb_.setTextDatum(middle_left);
  fb_.drawString("All agents", x + 28, y + h / 2);
  fb_.setTextDatum(top_left);
  for (int i = 0; i < MAXAGENTS; i++)
    if (i != focus && agents[i].used && agents[i].needsYou()) { fb_.fillCircle(x + w - 4, y + 4, 5, fb_.color565(255, 180, 50)); break; }
}

void Ui::drawFrame(bool connected, uint32_t now) {
  for (Agent& ag : agents)
    for (int i = ag.nmsg - 1; i >= 0; i--)
      if (ag.msgs[i].ttl && now - ag.msgs[i].at > ag.msgs[i].ttl * 1000UL) ag.nmsg = i;  // oldest expire first
  int n = count();
  if (focus >= 0 && (n < 2 || !agents[focus].used)) focus = -1;
  const Agent* v = view();
  // Nebula colours follow the state: greyscale with no agent, amber when the agent (or, on
  // the card list, any agent) needs you.
  bool presenceScreen = v && !v->completed && !v->asking && v->status == ST_IDLE;
  bool alarm = v ? presenceScreen && v->attn : anyNeedsYou();
  nebula_.mood(net.setup ? Nebula::M_NORMAL : !n ? Nebula::M_IDLE : alarm ? Nebula::M_ATTN : Nebula::M_NORMAL, now / 1000.0f);
  if (net.setup) drawSetup(now);
  else if (!n) drawIdle(now);
  else if (!v) drawAgents(now);
  else if (v->completed && !v->asking) drawDone(now);
  else if (v->status == ST_IDLE && !v->asking) drawPresence(now);
  else {
    drawStatus(now);
    if (v->asking) drawAsk(now);
  }
  if (!net.setup && n > 1 && v) drawBack();
  if (showingUpdate(connected)) drawUpdatePrompt();
  if (showingInfo(connected)) drawInfo(now);
  if (hold > 0) drawHold();
  fb_.pushSprite(&lcd_, 0, 0);
}
