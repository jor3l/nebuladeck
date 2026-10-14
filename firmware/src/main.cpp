#include <Arduino.h>
#include "display.h"
#include "proto.h"
#include "ui.h"

static Display lcd;
static Ui ui(lcd);
static Proto proto(ui);

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.setRotation(0);  // portrait, 320x480
  lcd.setBrightness(200);
  lcd.fillScreen(TFT_BLACK);
  if (!ui.begin()) {
    lcd.setTextColor(TFT_RED);
    lcd.drawString("framebuffer alloc failed", 10, 10);
    for (;;) delay(1000);
  }
}

void loop() {
  uint32_t now = millis();
  proto.poll(now);

  // Taps: act on the press edge.
  static bool down = false;
  int32_t x, y;
  bool touching = lcd.getTouch(&x, &y);
  bool conn = proto.connected(now);
  if (touching && !conn) {
    // Ripple on press, then a trail while dragging.
    static int32_t lx = -100, ly = -100;
    static uint32_t lastRip = 0;
    int32_t dx = x - lx, dy = y - ly;
    if (!down || (dx * dx + dy * dy > 18 * 18 && now - lastRip > 90)) {
      ui.ripple(x, y, now);
      lx = x; ly = y; lastRip = now;
    }
  }
  if (touching && !down && conn) {
    int c = ui.hitAsk(x, y);
    if (c >= 0) {
      proto.sendTap(ui.askId, c, ui.opts[c]);
      ui.asking = false;
      ui.status = ST_WORKING;
    }
  }
  down = touching;

  static uint32_t lastFrame = 0;
  if (now - lastFrame >= 40) {
    lastFrame = now;
    ui.drawFrame(conn, now);
  }
}
