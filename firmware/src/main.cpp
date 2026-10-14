#include <Arduino.h>
#include "api.h"
#include "display.h"
#include "net.h"
#include "ota.h"
#include "ui.h"

static Display lcd;
static Ui ui(lcd);
static Net net(ui.net);
static Api api(ui);
static Ota ota(ui);

static const uint32_t HOLD_MS = 5000;   // long-press on a nebula screen -> WiFi setup
static const uint32_t SLEEP_MS = 180000;  // no touch and no agent requests for this long -> screen off
static const uint32_t ATTN_ON_MS = 600000;  // "needs you" keeps the screen lit at most this long
static const int BRIGHTNESS = 200;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.setRotation(2);  // portrait, 320x480, USB port at the bottom
  lcd.setBrightness(BRIGHTNESS);
  lcd.fillScreen(TFT_BLACK);
  if (!ui.begin()) {
    lcd.setTextColor(TFT_RED);
    lcd.drawString("framebuffer alloc failed", 10, 10);
    for (;;) delay(1000);
  }
  net.begin();
  api.begin();
  ota.begin();
  api.onForgetWifi = [] { net.forget(); };
  api.onUpdate = [] { ota.requestInstall(); };
}

void loop() {
  uint32_t now = millis();
  net.poll(now);
  api.poll(now);
  ota.poll(now, ui.net.online);

  // Taps: act on the press edge.
  static bool down = false;
  static uint32_t holdStart = 0;
  static int32_t hx, hy;
  enum : uint8_t { SH_NONE, SH_INFO, SH_UPDATE };
  static uint8_t pressed = SH_NONE;  // which bottom sheet the finger went down on
  static int32_t sx, sy;
  static int32_t x0List, y0List;  // where a press on the card list started
  int32_t x, y;
  bool touching = lcd.getTouch(&x, &y);
  bool conn = !ui.net.setup && api.connected(now);

  // Screen sleep: fade the backlight out after SLEEP_MS of no touch and no agent requests,
  // unless something needs the user. A tap (or any request) wakes it; the waking tap is
  // swallowed so it can't press anything.
  static uint32_t lastTouch = 0;
  static int bright = BRIGHTNESS;
  static bool asleep = false, swallow = false;
  if (touching) lastTouch = now;
  uint32_t lastActive = (int32_t)(api.lastRequest - lastTouch) > 0 ? api.lastRequest : lastTouch;
  static uint32_t attnSince = 0;
  bool anyAsking = false, attn = false;
  for (const Agent& a : ui.agents) if (a.used) { anyAsking |= a.asking; attn |= a.attn; }
  if (!attn) attnSince = 0;
  else if (!attnSince) attnSince = now;
  bool keepOn = ui.net.setup || anyAsking || (attn && now - attnSince < ATTN_ON_MS) || ui.updatePct >= 0;
  bool idle = !keepOn && now - lastActive > SLEEP_MS;
  if (asleep && !idle) {
    asleep = false;
    bright = BRIGHTNESS;
    lcd.setBrightness(bright);
    swallow = touching;  // this tap only wakes the screen
  } else if (!asleep && idle) {
    // Fade over ~1 s. Backlight only: the panel's sleep mode isn't used, so touch stays live.
    int b = BRIGHTNESS - (int)((now - lastActive - SLEEP_MS) * BRIGHTNESS / 1000);
    if (b != bright) lcd.setBrightness(bright = max(0, b));
    asleep = bright == 0;
  } else if (!asleep && bright < BRIGHTNESS) {
    lcd.setBrightness(bright = BRIGHTNESS);  // activity during the fade
  }
  if (swallow && !touching) swallow = false;
  if (asleep || swallow) {
    down = touching;
    return;  // no drawing, no touch handling
  }
  // Several agents: drag the card list to scroll, tap a card to open that agent, tap
  // "All agents" to come back.
  bool list = !ui.net.setup && conn && !ui.view();
  static bool listPress = false;
  static int32_t ly0, lscroll0, lmove;
  if (touching && !down && list && !ui.info && y >= 80) { listPress = true; ly0 = y; lscroll0 = ui.scroll; lmove = 0; }
  if (listPress && touching) {
    lmove = max(lmove, (int32_t)abs(y - ly0));
    ui.scroll = max(0, min((int)(lscroll0 - (y - ly0)), ui.maxScroll()));
  } else if (listPress) {  // released
    listPress = false;
    if (lmove < 12) { int c = ui.hitCard(x0List, y0List); if (c >= 0) { ui.focus = c; ui.info = false; } }
  }
  if (listPress && !down) { x0List = x; y0List = y; }
  if (touching && !down && ui.hitBack(x, y)) ui.focus = -1;

  if (touching && ui.onNebula(conn) && !ui.info && !pressed && !list && !listPress) {
    // Ripple on press, then a trail while dragging.
    static int32_t lx = -100, ly = -100;
    static uint32_t lastRip = 0;
    int32_t dx = x - lx, dy = y - ly;
    if (!down || (dx * dx + dy * dy > 18 * 18 && now - lastRip > 90)) {
      ui.ripple(x, y, now);
      lx = x; ly = y; lastRip = now;
    }
  }
  if (touching && !down && ui.hitCancel(x, y)) net.cancelSetup();

  // Bottom sheets (device info, update prompt): swipe down to dismiss, tap outside the info
  // sheet to close it. Buttons act on release, so a swipe never presses one.
  if (!ui.showingInfo(conn)) ui.info = false;  // an ask or task took over
  bool infoSheet = ui.info;
  bool updateSheet = ui.showingUpdate(conn);
  if (touching && !down && (infoSheet || updateSheet)) {
    int b = infoSheet ? ui.hitInfo(x, y) : ui.hitUpdate(x, y);
    if (b == -2 && infoSheet) ui.info = false;
    else if (b != -2) { pressed = infoSheet ? SH_INFO : SH_UPDATE; sx = x; sy = y; }
  } else if (touching && !down && !ui.net.setup && ui.onNebula(conn) && ui.hitLogo(x, y)) {
    ui.info = true;
    ui.forgetArmed = 0;
    infoSheet = true;
  }
  if (pressed && touching) {
    ui.sheetDrag = max(0, (int)(y - sy));
  } else if (pressed) {  // released
    int drag = ui.sheetDrag;
    ui.sheetDrag = 0;
    bool still = pressed == SH_INFO ? ui.showingInfo(conn) : ui.showingUpdate(conn);
    if (still && drag > 70) {
      if (pressed == SH_INFO) ui.info = false;
      else ui.updatePrompt = false;  // later: comes back with the next check
    } else if (still && drag < 15 && pressed == SH_INFO) {
      int b = ui.hitInfo(sx, sy);
      if (b == 0 && ui.update.length()) { ui.info = false; ota.requestInstall(); }
      else if (b == 0) ota.checkNow();
      else if (b == 1 && ui.forgetArmed && now - ui.forgetArmed < 4000) { ui.info = false; ui.forgetArmed = 0; net.forget(); }
      else if (b == 1) ui.forgetArmed = now;
    } else if (still && drag < 15) {
      int b = ui.hitUpdate(sx, sy);
      if (b == 0) ota.requestInstall();
      else if (b == 1) ota.skip();
    }
    pressed = SH_NONE;
  }
  bool sheet = pressed || infoSheet || updateSheet;
  Agent* v = ui.view();
  if (touching && !down && v && v->asking) {
    int c = ui.hitAsk(x, y);
    if (c >= 0) api.answer(*v, c);
  }

  // Hold still on a nebula screen to reach WiFi setup.
  if (touching && !ui.net.setup && !sheet && !list && ui.onNebula(conn)) {
    if (!down) { holdStart = now; hx = x; hy = y; }
    if ((x - hx) * (x - hx) + (y - hy) * (y - hy) > 30 * 30) holdStart = 0;  // a drag, not a hold
    ui.hold = holdStart && now - holdStart > 1000 ? (now - holdStart - 1000) / float(HOLD_MS - 1000) : 0;
    if (ui.hold >= 1) {
      ui.hold = 0;
      holdStart = 0;
      net.startSetup();
    }
  } else {
    ui.hold = 0;
    holdStart = 0;
  }
  down = touching;

  static uint32_t lastFrame = 0;
  if (now - lastFrame >= 30) {
    lastFrame = now;
    uint32_t t0 = millis();
    ui.drawFrame(conn, now);
    ui.frameMs = millis() - t0;
    if (ui.frameMs > ui.frameMaxMs) ui.frameMaxMs = ui.frameMs;
  }
}
