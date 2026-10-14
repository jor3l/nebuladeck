#pragma once
#include <Arduino.h>
#include "display.h"
#include "nebula.h"

enum Status : uint8_t { ST_IDLE, ST_WORKING, ST_WAITING, ST_DONE, ST_ERROR };
enum Level : uint8_t { LV_INFO, LV_WARN, LV_ERROR, LV_STEP };  // LV_STEP: a tool step from the hooks
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
  uint16_t count = 1;  // LV_STEP: repeats of the same step
};

enum AskState : uint8_t { ASK_NONE, ASK_PENDING, ASK_ANSWERED, ASK_CANCELLED };

// One connected agent, identified by name + project (e.g. "Claude" in "nebuladeck"). The
// device keeps each one's status, task, activity history, question and summary by itself.
struct Agent {
  static constexpr int MAXMSG = 12;
  static constexpr int MAXOPT = 4;
  bool used = false;
  String name, project;
  uint32_t lastSeen = 0;  // millis() of its last request; the lease runs from here
  // Task.
  String title;
  uint8_t status = ST_IDLE;
  float progress = -1;     // <0 = none
  uint32_t taskStart = 0;  // millis() when the current task began; activity is stamped relative to it
  Msg msgs[MAXMSG];        // newest first
  int nmsg = 0;
  // Question.
  bool asking = false;
  String askId, askPrompt, opts[MAXOPT];
  int nopts = 0;
  uint8_t askState = ASK_NONE, beforeAsk = ST_IDLE;  // status to return to afterwards
  uint32_t askDeadline = 0;
  String askLabel;
  int askChoice = -1;
  // Presence (no task): secondary line on the nebula.
  String presence;
  bool attn = false;
  // Finished task: summary and token card.
  bool completed = false;
  String summary;
  uint32_t tokIn = 0, tokOut = 0;
  int32_t durationS = -1;  // <0 = unknown

  void clear();
  void pushMsg(const String& text, uint8_t level, uint32_t ttl, uint32_t now);
  // A hook-reported tool step during a task: one line on top, repeats counted ("Editing x3").
  void pushStep(const String& text, uint32_t now);
  bool needsYou() const { return asking || attn; }
  // What the agent is doing, in one line (for its card in the list).
  String line() const;
};

class Ui {
 public:
  static constexpr int MAXAGENTS = 6;
  static constexpr int MAXOPT = Agent::MAXOPT;

  Agent agents[MAXAGENTS];
  int focus = -1;   // with several agents: the one opened from its card, -1 = the card list
  int scroll = 0;   // card list scroll offset, px
  int count() const;
  // The agent the detail screens show: the only one, or the focused one. nullptr = card list.
  Agent* view();
  const Agent* view() const;
  bool anyNeedsYou() const;
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
  int sheetDrag = 0;         // px a bottom sheet is being swiped down

  explicit Ui(LGFX_Device& lcd) : lcd_(lcd), fb_(&lcd), qr_(&lcd) {}
  bool begin();
  // Card list (2+ agents): the card at a point (index into agents[]), or -1; scroll bounds.
  int hitCard(int x, int y) const;
  int maxScroll() const;
  // "‹ All agents" on an opened agent's screens.
  bool hitBack(int x, int y) const { return count() > 1 && focus >= 0 && x > W_ - 140 && y < 44; }
  // Returns the tapped ask option, or -1.
  int hitAsk(int x, int y) const;
  bool hitCancel(int x, int y) const;
  // Update prompt: 0 = update, 1 = skip, -1 = inside the sheet, -2 = outside.
  int hitUpdate(int x, int y) const;
  bool showingUpdate(bool connected) const { return updatePrompt && !showingInfo(connected) && !net.setup && onNebula(connected); }
  bool showingInfo(bool connected) const { return info && !net.setup && onNebula(connected); }
  bool hitLogo(int x, int y) const { return x > 50 && x < W_ - 50 && y > 16 && y < 72; }
  // Info sheet: 0 = check/install, 1 = forget, -1 = inside the sheet, -2 = outside (close).
  int hitInfo(int x, int y) const;
  // Install progress, drawn straight to the panel (the main loop is blocked meanwhile).
  void drawUpdating();
  void drawFrame(bool connected, uint32_t now);
  bool onNebula(bool connected) const {
    const Agent* a = view();
    return net.setup || !connected || !a || (!a->asking && (a->completed || a->status == ST_IDLE));
  }
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
  void drawAgents(uint32_t now);
  void drawBack();
  static constexpr int W_ = 320;

  LGFX_Device& lcd_;
  lgfx::LGFX_Sprite fb_;
  Nebula nebula_;
  lgfx::LGFX_Sprite qr_;
  String qrFor_;
};
