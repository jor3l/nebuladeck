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
  // Signed: requests handled inside poll(now) stamp lastSeen_ with a slightly later millis()
  // than `now`; unsigned, that difference wrapped to ~49 days and ended the session (and its ask).
  bool connected(uint32_t now) const { return lastSeen_ && (int32_t)(now - lastSeen_) < (int32_t)LEASE_MS; }
  void answer(int choice);  // an ask option was tapped
  std::function<void()> onForgetWifi;
  std::function<void()> onUpdate;  // install the available release (ui.update)

 private:
  enum AskState : uint8_t { ASK_NONE, ASK_PENDING, ASK_ANSWERED, ASK_CANCELLED };
  void route();
  void info();
  void askStatus();
  int apply(const char* t, JsonDocument& in, JsonDocument& out, uint32_t now);
  void cancelAsk();

  Ui& ui_;
  uint32_t lastSeen_ = 0, askDeadline_ = 0;
  uint8_t askState_ = ASK_NONE;
  int askChoice_ = -1;
  String askLabel_;
};
