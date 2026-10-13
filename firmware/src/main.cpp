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
  if (touching && !down) {
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
    ui.drawFrame(proto.connected(now), now);
  }
}
