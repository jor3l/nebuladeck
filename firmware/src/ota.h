#pragma once
#include <Arduino.h>
#include "ui.h"

// Over-the-air updates from the latest GitHub release. Checks in a background task
// (TLS would stall the animation), asks on screen, installs on request and reboots.
class Ota {
 public:
  explicit Ota(Ui& ui) : ui_(ui) {}
  void begin();
  void poll(uint32_t now, bool online);
  void requestInstall() { want_ = ui_.update.length() > 0; }
  void skip();
  void checkNow() { nextCheck_ = millis(); }

 private:
  static void checkTask(void* self);
  bool check();
  void install();

  Ui& ui_;
  String skipped_, url_;
  uint32_t nextCheck_ = 15000;  // first check shortly after boot
  volatile bool checking_ = false, found_ = false;
  String foundTag_, foundUrl_;
  bool want_ = false;
};
