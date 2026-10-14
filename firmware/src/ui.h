#pragma once
#include <Arduino.h>
#include "display.h"
#include "nebula.h"

enum Status : uint8_t { ST_IDLE, ST_WORKING, ST_WAITING, ST_DONE, ST_ERROR };
enum Level : uint8_t { LV_INFO, LV_WARN, LV_ERROR };
enum Tone : uint8_t { T_IDLE, T_BUSY, T_OK, T_ERR };
enum UpdateCheck : uint8_t { UC_IDLE, UC_CHECKING, UC_CURRENT, UC_FAILED };

// Network and setup state shown on the idle and welcome screens (written by Net).
struct NetInfo {
  bool setup = false;      // welcome/QR screen with BLE on
  bool online = false;     // joined, API reachable
  bool canCancel = false;  // setup can go back to a saved network
  String ssid, ip, host, bleName, url;
  int rssi = 0;
  String status, detail;   // setup screen lines
  uint8_t tone = T_IDLE;
};

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
  // Presence (connected, no task): agent name + secondary text on the nebula.
  String agent = "Claude", presence;
  bool attn = false;
  // Completed task: shown on the nebula as "Agent idle" with a token card.
  bool completed = false;
  String summary;
  uint32_t tokIn = 0, tokOut = 0;
  int32_t durationS = -1;  // <0 = unknown
  NetInfo net;
  float hold = 0;  // long-press progress towards WiFi setup, 0-1
  // Firmware update: newer release tag, on-screen prompt, install progress.
  String update, updateError;
  bool updatePrompt = false;
  int updatePct = -1;  // >=0 while installing
  volatile uint8_t updateCheck = UC_IDLE;  // written by the OTA check task
  // Device info sheet (tap the logo): version, network, updates, forget WiFi.
  bool info = false;
  uint32_t forgetArmed = 0;  // first tap on Forget; a second tap within 4 s confirms

  explicit Ui(LGFX_Device& lcd) : lcd_(lcd), fb_(&lcd), qr_(&lcd) {}
  bool begin();
  void reset();
  void pushMsg(const String& text, uint8_t level, uint32_t ttl, uint32_t now);
  // Returns the tapped ask option, or -1.
  int hitAsk(int x, int y) const;
  bool hitCancel(int x, int y) const;
  // Update prompt buttons: 0 = update, 1 = skip, -1 = miss.
  int hitUpdate(int x, int y) const;
  bool showingUpdate(bool connected) const { return updatePrompt && !showingInfo(connected) && !net.setup && onNebula(connected); }
  bool showingInfo(bool connected) const { return info && !net.setup && onNebula(connected); }
  bool hitLogo(int x, int y) const { return x > 50 && x < W_ - 50 && y > 16 && y < 72; }
  // Info sheet: 0 = check/install, 1 = forget, -1 = inside the sheet, -2 = outside (close).
  int hitInfo(int x, int y) const;
  // Install progress, drawn straight to the panel (the main loop is blocked meanwhile).
  void drawUpdating();
  void drawFrame(bool connected, uint32_t now);
  bool onNebula(bool connected) const { return net.setup || !connected || (!asking && (completed || status == ST_IDLE)); }
  // Touch on the idle nebula.
  void ripple(int x, int y, uint32_t now) { nebula_.ripple(x, y, now / 1000.0f); }

 private:
  void drawIdle(uint32_t now);
  void drawDone(uint32_t now);
  void drawPresence(uint32_t now);
  void drawStatus(uint32_t now);
  void drawAsk(uint32_t now);
  void drawSetup(uint32_t now);
  void drawHold();
  void drawUpdatePrompt();
  void drawInfo(uint32_t now);
  static constexpr int W_ = 320;

  LGFX_Device& lcd_;
  lgfx::LGFX_Sprite fb_;
  Nebula nebula_;
  lgfx::LGFX_Sprite qr_;
  String qrFor_;
};
