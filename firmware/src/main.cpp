#include <Arduino.h>
#include "display.h"

static Display lcd;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.setRotation(0);  // portrait, 320x480
  lcd.setBrightness(200);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(10, 10);
  lcd.printf("NebulaDeck\n%dx%d", lcd.width(), lcd.height());
  Serial.printf("boot ok %dx%d psram=%u\n", lcd.width(), lcd.height(), ESP.getPsramSize());
}

void loop() {
  static uint32_t last = 0;
  int32_t x, y;
  if (lcd.getTouch(&x, &y)) {
    lcd.fillCircle(x, y, 6, TFT_CYAN);
    Serial.printf("touch %d,%d\n", x, y);
  }
  if (millis() - last > 1000) {
    last = millis();
    Serial.printf("alive %lu\n", millis() / 1000);
  }
}
