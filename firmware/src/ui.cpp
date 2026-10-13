#include "ui.h"
#include <vector>

static const int W = 320, H = 480;

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

void Ui::reset() {
  project = ""; title = ""; status = ST_IDLE; progress = -1;
  nmsg = 0; asking = false; nopts = 0;
}

void Ui::pushMsg(const String& text, uint8_t level, uint32_t ttl, uint32_t now) {
  for (int i = MAXMSG - 1; i > 0; i--) msgs[i] = msgs[i - 1];
  msgs[0].text = text; msgs[0].level = level; msgs[0].at = now; msgs[0].ttl = ttl;
  if (nmsg < MAXMSG) nmsg++;
}

// Bottom sheet geometry shared by drawing and hit-testing.
static int sheetTop(int nopts) { return H - (110 + nopts * 60 + 8); }
static void optRect(int nopts, int i, int& x, int& y, int& w, int& h) {
  x = 16; w = W - 32; h = 52; y = sheetTop(nopts) + 110 + i * 60;
}

int Ui::hitAsk(int px, int py) const {
  if (!asking) return -1;
  for (int i = 0; i < nopts; i++) {
    int x, y, w, h;
    optRect(nopts, i, x, y, w, h);
    if (px >= x && px < x + w && py >= y && py < y + h) return i;
  }
  return -1;
}

void Ui::drawIdle(uint32_t now) {
  float t = now / 1000.0f;
  nebula_.draw(fb_, t);
  float pulse = 0.5f + 0.5f * sinf(t * 1.4f);
  fb_.setTextDatum(middle_center);
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextColor(fb_.color565(150, 160, 210));
  fb_.drawString("N E B U L A D E C K", W / 2, 44);

  int b = 150 + (int)(pulse * 105);
  fb_.setFont(&fonts::FreeSans12pt7b);
  fb_.setTextColor(fb_.color565(b, b, 255));
  fb_.drawString("Agent not connected", W / 2, H / 2);
  fb_.setFont(&fonts::FreeSans9pt7b);
  fb_.setTextColor(fb_.color565(120, 130, 180));
  fb_.drawString("waiting for host", W / 2, H / 2 + 30);
}

void Ui::drawStatus(uint32_t now) {
  float t = now / 1000.0f;
  uint16_t sc = statusColor(fb_, status);
  fb_.fillScreen(fb_.color565(7, 9, 20));
  fb_.setTextDatum(top_left);

  // Header: live dot + project.
  int pr = 5 + (int)(2.0f * (0.5f + 0.5f * sinf(t * 3.0f)));
  fb_.fillCircle(24, 26, pr, fb_.color565(70, 220, 140));
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(230, 235, 255));
  fb_.drawString(project.length() ? project : "NebulaDeck", 44, 12);

  // Hero card.
  fb_.fillRoundRect(16, 56, 288, 160, 16, fb_.color565(16, 20, 42));
  fb_.drawRoundRect(16, 56, 288, 160, 16, fb_.color565(36, 44, 88));
  fb_.fillRoundRect(32, 74, 6, 16, 3, sc);
  fb_.setFont(&fonts::FreeSansBold9pt7b);
  fb_.setTextColor(sc);
  fb_.drawString(statusLabel(status), 46, 72);
  fb_.setFont(&fonts::FreeSansBold18pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  wrapText(fb_, title.length() ? title : "Ready", 32, 100, 256, 32, 3);

  // Progress / activity bar.
  if (progress >= 0 || status == ST_WORKING) {
    int bx = 32, by = 198, bw = 256, bh = 6;
    fb_.fillRoundRect(bx, by, bw, bh, 3, fb_.color565(32, 38, 72));
    if (progress >= 0) {
      int fw = (int)(bw * min(progress, 1.0f));
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
  for (int i = 0; i < nmsg && i < 4; i++) {
    int y = 254 + i * 54;
    fb_.fillRoundRect(16, y, 288, 48, 10, fb_.color565(16, 20, 42));
    fb_.fillRoundRect(16, y + 6, 4, 36, 2, levelColor(fb_, msgs[i].level));
    fb_.setFont(&fonts::FreeSans9pt7b);
    fb_.setTextColor(fb_.color565(215, 222, 245));
    wrapText(fb_, msgs[i].text, 30, y + 4, 232, 20, 2);
    fb_.setTextDatum(top_right);
    fb_.setTextColor(fb_.color565(100, 110, 160));
    fb_.drawString(ago(now - msgs[i].at), 298, y + 4);
    fb_.setTextDatum(top_left);
  }
}

void Ui::drawAsk(uint32_t) {
  fb_.fillRectAlpha(0, 0, W, H, 170, fb_.color565(2, 3, 10));
  int top = sheetTop(nopts);
  fb_.fillRoundRect(0, top, W, H - top + 20, 22, fb_.color565(20, 25, 52));
  fb_.fillRoundRect(W / 2 - 22, top + 8, 44, 4, 2, fb_.color565(70, 80, 130));
  fb_.setTextDatum(top_left);
  fb_.setFont(&fonts::FreeSansBold12pt7b);
  fb_.setTextColor(fb_.color565(240, 243, 255));
  wrapText(fb_, askPrompt, 20, top + 22, W - 40, 26, 3);
  for (int i = 0; i < nopts; i++) {
    int x, y, w, h;
    optRect(nopts, i, x, y, w, h);
    uint16_t c = i == 0 ? fb_.color565(60, 200, 255) : fb_.color565(36, 44, 88);
    fb_.fillRoundRect(x, y, w, h, 14, c);
    fb_.setTextDatum(middle_center);
    fb_.setFont(&fonts::FreeSansBold12pt7b);
    fb_.setTextColor(i == 0 ? fb_.color565(4, 12, 28) : fb_.color565(230, 235, 255));
    fb_.drawString(opts[i], x + w / 2, y + h / 2);
  }
}

void Ui::drawFrame(bool connected, uint32_t now) {
  for (int i = nmsg - 1; i >= 0; i--)
    if (msgs[i].ttl && now - msgs[i].at > msgs[i].ttl * 1000UL) nmsg = i;  // oldest expire first
  if (!connected) drawIdle(now);
  else {
    drawStatus(now);
    if (asking) drawAsk(now);
  }
  fb_.pushSprite(&lcd_, 0, 0);
}
