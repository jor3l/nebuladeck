#pragma once
#include <Arduino.h>
#include "ui.h"

// Newline-delimited JSON over USB serial; see docs/PROTOCOL.md.
class Proto {
 public:
  explicit Proto(Ui& ui) : ui_(ui) {}
  void poll(uint32_t now);
  bool connected(uint32_t now) const { return lastPing_ && now - lastPing_ < 3000; }
  void sendTap(const String& id, int choice, const String& label);

 private:
  void handle(const String& line, uint32_t now);
  Ui& ui_;
  String buf_;
  uint32_t lastPing_ = 0;
};
