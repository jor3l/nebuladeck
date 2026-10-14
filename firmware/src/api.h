#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include "ui.h"

// HTTP JSON API on port 80 (http://nebuladeck.local); see docs/PROTOCOL.md.
class Api {
 public:
  static constexpr uint32_t LEASE_MS = 30UL * 60 * 1000;  // no requests for this long -> idle nebula

  explicit Api(Ui& ui) : ui_(ui) {}
  void begin();
  void poll(uint32_t now);
  bool connected(uint32_t now) const;  // any agent connected
  void answer(Agent& a, int choice);   // an ask option was tapped
  uint32_t lastRequest = 0;  // millis() of the last command (wakes a sleeping screen)
  std::function<void()> onForgetWifi;
  std::function<void()> onUpdate;  // install the available release (ui.update)

 private:
  void route();
  void info();
  void askStatus();
  int apply(const char* t, JsonDocument& in, JsonDocument& out, uint32_t now);
  Agent* resolve(JsonDocument& doc, uint32_t now, bool create);
  Agent* findAsk(const String& id);
  void cancelAsk(Agent& a, const char* why);
  // Recent events for debugging (why questions ended, agents came and went); in /api/info.
  static constexpr int LOGN = 12;
  String log_[LOGN];
  int logNext_ = 0;
  void note(const String& s);

  Ui& ui_;
};
