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

static const uint32_t HOLD_MS = 5000;  // long-press on a nebula screen -> WiFi setup

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.setRotation(2);  // portrait, 320x480, USB port at the bottom
  lcd.setBrightness(200);
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
  int32_t x, y;
  bool touching = lcd.getTouch(&x, &y);
  bool conn = !ui.net.setup && api.connected(now);
  if (touching && ui.onNebula(conn) && !ui.info) {
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

  // Device info sheet: tap the logo to open, outside to close.
  if (!ui.showingInfo(conn)) ui.info = false;  // an ask or task took over
  bool infoSheet = ui.info;
  if (touching && !down && infoSheet) {
    int b = ui.hitInfo(x, y);
    if (b == -2) ui.info = false;
    else if (b == 0 && ui.update.length()) { ui.info = false; ota.requestInstall(); }
    else if (b == 0) ota.checkNow();
    else if (b == 1 && ui.forgetArmed && now - ui.forgetArmed < 4000) { ui.info = false; ui.forgetArmed = 0; net.forget(); }
    else if (b == 1) ui.forgetArmed = now;
  } else if (touching && !down && !ui.net.setup && ui.onNebula(conn) && ui.hitLogo(x, y)) {
    ui.info = true;
    ui.forgetArmed = 0;
    infoSheet = true;
  }
  bool updateSheet = ui.showingUpdate(conn);
  if (touching && !down && updateSheet) {
    int b = ui.hitUpdate(x, y);
    if (b == 0) ota.requestInstall();
    else if (b == 1) ota.skip();
  }
  if (touching && !down && ui.asking && conn) {
    int c = ui.hitAsk(x, y);
    if (c >= 0) api.answer(c);
  }

  // Hold still on a nebula screen to reach WiFi setup.
  if (touching && !ui.net.setup && !updateSheet && !infoSheet && ui.onNebula(conn)) {
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
    ui.drawFrame(conn, now);
  }
}
