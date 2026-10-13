#pragma once
#include <Arduino.h>
#include "display.h"
#include "nebula.h"

enum Status : uint8_t { ST_IDLE, ST_WORKING, ST_WAITING, ST_DONE, ST_ERROR };
enum Level : uint8_t { LV_INFO, LV_WARN, LV_ERROR };

struct Msg {
  String text;
  uint8_t level = LV_INFO;
  uint32_t at = 0;
  uint32_t ttl = 0;  // seconds, 0 = stays
};

class Ui {
 public:
  static constexpr int MAXMSG = 6;
  static constexpr int MAXOPT = 4;

  // Content (set by the protocol layer).
  String project, title;
  uint8_t status = ST_IDLE;
  float progress = -1;  // <0 = none
  Msg msgs[MAXMSG];     // newest first
  int nmsg = 0;
  bool asking = false;
  String askId, askPrompt, opts[MAXOPT];
  int nopts = 0;

  explicit Ui(LGFX_Device& lcd) : lcd_(lcd), fb_(&lcd), nebula_(&lcd) {}
  bool begin();
  void reset();
  void pushMsg(const String& text, uint8_t level, uint32_t ttl, uint32_t now);
  // Returns the tapped ask option, or -1.
  int hitAsk(int x, int y) const;
  void drawFrame(bool connected, uint32_t now);

 private:
  void drawIdle(uint32_t now);
  void drawStatus(uint32_t now);
  void drawAsk(uint32_t now);

  LGFX_Device& lcd_;
  lgfx::LGFX_Sprite fb_;
  Nebula nebula_;
};
